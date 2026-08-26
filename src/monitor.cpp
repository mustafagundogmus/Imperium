#include "monitor.h"

#include <QStorageInfo>
#include <QTimer>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
// netioapi.h (reached through iphlpapi.h) needs the winsock2 address types, and those
// have to land before windows.h pulls in the older winsock.h.
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <iphlpapi.h>
#endif

namespace {

#ifdef Q_OS_WIN
quint64 toUInt64(const FILETIME &ft)
{
    return (quint64(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
#endif

} // namespace

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    m_cpu.reserve(HistorySize);
    m_ram.reserve(HistorySize);

#ifdef Q_OS_WIN
    // Not GetSystemInfo: dwNumberOfProcessors counts only the group the calling thread is
    // in, and caps at 64. The physical count below spans every group, so on a machine with
    // more than one the page showed a thread count smaller than its own core count.
    m_logicalCores = int(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));

    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (length > 0) {
        QByteArray buffer(int(length), Qt::Uninitialized);
        auto *first = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, first, &length)) {
            DWORD offset = 0;
            while (offset < length) {
                auto *info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data() + offset);
                if (info->Relationship == RelationProcessorCore)
                    ++m_physicalCores;
                offset += info->Size;
            }
        }
    }
#endif

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] { poll(); });
}

void SystemMonitor::start(int intervalMs)
{
    // Prime the delta counters without recording a sample: CPU and network are both
    // rate measurements, so the very first read has nothing to compare against and
    // would otherwise push a bogus 0% into the history.
    m_netClock.start();
    readCpuPercent();
    Sample discard;
    readNetwork(discard);

    poll(/*record=*/false);       // memory and disk are absolute — show them at once
    m_timer->start(intervalMs);
}

void SystemMonitor::stop()
{
    m_timer->stop();
}

qreal SystemMonitor::readCpuPercent()
{
#ifdef Q_OS_WIN
    FILETIME idleTime{}, kernelTime{}, userTime{};
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return m_latest.cpuPercent;

    const quint64 idle = toUInt64(idleTime);
    const quint64 kernel = toUInt64(kernelTime);   // includes idle
    const quint64 user = toUInt64(userTime);

    if (!m_haveCpuBaseline) {
        m_prevIdle = idle;
        m_prevKernel = kernel;
        m_prevUser = user;
        m_haveCpuBaseline = true;
        return 0.0;
    }

    const quint64 idleDelta = idle - m_prevIdle;
    const quint64 totalDelta = (kernel - m_prevKernel) + (user - m_prevUser);

    m_prevIdle = idle;
    m_prevKernel = kernel;
    m_prevUser = user;

    if (totalDelta == 0)
        return m_latest.cpuPercent;
    return qBound(0.0, 100.0 * (1.0 - qreal(idleDelta) / qreal(totalDelta)), 100.0);
#else
    return 0.0;
#endif
}

void SystemMonitor::readNetwork(Sample &sample)
{
#ifdef Q_OS_WIN
    MIB_IF_TABLE2 *table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table)
        return;

    quint64 in = 0, out = 0;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2 &row = table->Table[i];
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        if (row.OperStatus != IfOperStatusUp)
            continue;
        in += row.InOctets;
        out += row.OutOctets;
    }
    FreeMibTable(table);

    const qint64 elapsed = m_netClock.restart();
    if (!m_haveNetBaseline || elapsed <= 0) {
        m_prevIn = in;
        m_prevOut = out;
        m_haveNetBaseline = true;
        return;
    }

    const qreal seconds = elapsed / 1000.0;
    sample.downBytesPerSec = in > m_prevIn ? quint64((in - m_prevIn) / seconds) : 0;
    sample.upBytesPerSec = out > m_prevOut ? quint64((out - m_prevOut) / seconds) : 0;
    m_prevIn = in;
    m_prevOut = out;
#else
    Q_UNUSED(sample);
#endif
}

void SystemMonitor::poll(bool record)
{
    Sample sample;
    sample.cpuPercent = record ? readCpuPercent() : 0.0;

#ifdef Q_OS_WIN
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        sample.ramTotal = memory.ullTotalPhys;
        sample.ramUsed = memory.ullTotalPhys - memory.ullAvailPhys;
        sample.ramPercent = memory.ullTotalPhys > 0
                                ? 100.0 * qreal(sample.ramUsed) / qreal(sample.ramTotal)
                                : 0.0;
    }
#endif

    const QStorageInfo storage = QStorageInfo::root();
    if (storage.isValid() && storage.bytesTotal() > 0) {
        sample.diskTotal = quint64(storage.bytesTotal());
        sample.diskUsed = sample.diskTotal - quint64(storage.bytesAvailable());
    }

    if (record)
        readNetwork(sample);

    m_latest = sample;

    if (record) {
        m_cpu.append(sample.cpuPercent);
        m_ram.append(sample.ramPercent);
        while (m_cpu.size() > HistorySize)
            m_cpu.removeFirst();
        while (m_ram.size() > HistorySize)
            m_ram.removeFirst();
    }

    Q_EMIT sampled(sample);
}
