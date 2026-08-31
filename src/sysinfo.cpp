#include "sysinfo.h"

#include "i18n.h"
#include "registry.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QVariant>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <iphlpapi.h>
#  include <lmaccess.h>
#  include <lmapibuf.h>
#  include <winioctl.h>
#  include <psapi.h>
#endif

namespace SysInfo {
namespace {

const QString Unknown = QStringLiteral("—");

QSettings hklm(const QString &path)
{
    return Registry::openKey(Registry::Hive::HKLM, path);
}

QSettings hkcu(const QString &path)
{
    return Registry::openKey(Registry::Hive::HKCU, path);
}

QString orUnknown(const QString &s)
{
    const QString t = s.trimmed();
    return t.isEmpty() ? Unknown : t;
}

/// "1.24 TB" / "512 GB" — one decimal for TB, none below.
QString formatBytes(quint64 bytes)
{
    constexpr qreal GB = 1024.0 * 1024.0 * 1024.0;
    const qreal gb = bytes / GB;
    if (gb >= 1024.0)
        return QStringLiteral("%1 TB").arg(QLocale().toString(gb / 1024.0, 'f', 2));
    return QStringLiteral("%1 GB").arg(qRound(gb));
}

#ifdef Q_OS_WIN

bool isElevated()
{
    return Registry::isElevated();
}

/// TPM version via the TPM Base Services, loaded on demand so tbs.dll is not a hard
/// dependency on editions that lack it.
QString tpmVersion()
{
    struct TbsDeviceInfo
    {
        UINT32 structVersion;
        UINT32 tpmVersion;
        UINT32 tpmInterfaceType;
        UINT32 tpmImpRevision;
    };
    using GetDeviceInfoFn = UINT32(WINAPI *)(UINT32, PVOID);

    // Pinned to System32 rather than left to the default search order, which looks in the
    // directory the executable was started from first. Arbitrium is one portable file that
    // people run out of Downloads, and it always runs elevated: a tbs.dll next to it would
    // otherwise be loaded into an administrator process. Only these call sites are pinned —
    // a process-wide SetDefaultDllDirectories would also apply to the shell extensions the
    // native file dialogs load, which is not a trade this needs to make.
    HMODULE tbs = LoadLibraryExW(L"tbs.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!tbs)
        return Locale::tr(QStringLiteral("sys.none"));

    QString result = Locale::tr(QStringLiteral("sys.none"));
    if (auto fn = reinterpret_cast<GetDeviceInfoFn>(
            reinterpret_cast<void *>(GetProcAddress(tbs, "Tbsi_GetDeviceInfo")))) {
        TbsDeviceInfo info{};
        if (fn(sizeof(info), &info) == 0) {
            if (info.tpmVersion == 2)
                result = QStringLiteral("2.0");
            else if (info.tpmVersion == 1)
                result = QStringLiteral("1.2");
        }
    }
    FreeLibrary(tbs);
    return result;
}

struct CoreCounts
{
    int physical = 0;
    int logical = 0;
};

CoreCounts coreCounts()
{
    CoreCounts c;

    // Not GetSystemInfo: dwNumberOfProcessors counts only the processor group the calling
    // thread happens to be in and stops at 64, while the physical count below walks every
    // group. Two scopes feeding one "%1 çekirdek / %2 iş parçacığı" label meant a machine
    // with more than one group reported fewer threads than it had cores.
    c.logical = int(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));

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
                    ++c.physical;
                offset += info->Size;
            }
        }
    }
    return c;
}

/// The raw SMBIOS table, exactly as GetSystemFirmwareTable returns it: a RawSMBIOSData
/// header of four bytes of version info and a DWORD length, then the structures.
///
/// Fetched once. Three call sites used to ask the firmware for this same table and parse
/// it separately, which is a waste but also three chances to disagree. Holding it is not
/// a cache that can go stale: the firmware builds the table at boot and hands the OS a
/// copy, so it cannot change while this process is alive. Empty when the table is
/// unavailable — a virtual machine may have none at all.
const QByteArray &smbiosTable()
{
    static const QByteArray table = [] {
        // GetSystemFirmwareTable's provider signature 'RSMB', spelled out so it is not a
        // multi-character literal with implementation-defined ordering.
        constexpr DWORD RawSmbios = 0x52534D42;

        const DWORD size = GetSystemFirmwareTable(RawSmbios, 0, nullptr, 0);
        if (size == 0)
            return QByteArray();
        QByteArray raw(int(size), Qt::Uninitialized);
        if (GetSystemFirmwareTable(RawSmbios, 0, raw.data(), size) != size)
            return QByteArray();
        return raw;
    }();
    return table;
}

/// Everything the SMBIOS type 17 (Memory Device) structures say, parsed once.
struct MemoryModules
{
    /// Sum of the populated modules' capacities — what is physically installed. Zero when
    /// the table is missing, holds no type 17, or reports a size this cannot add up.
    quint64 installedBytes = 0;
    QString kind;          ///< "DDR5-5600" — empty when the modules do not agree on a type
    int slotCount = 0;
    int filledCount = 0;
};

const MemoryModules &memoryModules()
{
    static const MemoryModules parsed = [] {
        MemoryModules m;
        const QByteArray &raw = smbiosTable();
        if (raw.size() < 8)
            return m;

        const quint32 tableLength = *reinterpret_cast<const quint32 *>(raw.constData() + 4);
        const uchar *p = reinterpret_cast<const uchar *>(raw.constData()) + 8;
        const uchar *end = p + qMin<quint32>(tableLength, quint32(raw.size() - 8));

        QString type;
        bool typesDisagree = false;
        // Unsigned, and wider than the WORD it usually comes from, because the 3.3 extended
        // speed fields below are DWORDs.
        quint32 speed = 0;
        bool sizeUnknown = false;

        while (p + 4 <= end) {
            const uchar structType = p[0];
            const uchar headerLength = p[1];
            if (headerLength < 4)
                break;
            const uchar *structEnd = p + headerLength;
            // A structure whose declared length runs past the table is a truncated read,
            // not a structure: every field below is addressed off p and would be reaching
            // outside the buffer. The old walkers checked only that four bytes were left.
            if (structEnd > end)
                break;

            // Field offsets and their minimum structure lengths are from the SMBIOS
            // specification, DSP0134 3.7.0, section 7.18 "Memory Device (Type 17)".
            if (structType == 17 && headerLength >= 0x0E) {
                ++m.slotCount;
                const quint16 sizeField = *reinterpret_cast<const quint16 *>(p + 0x0C);
                if (sizeField != 0) {   // populated slot; 0 means nothing in this socket
                    ++m.filledCount;

                    // 7.18.5 Size, offset 0x0C, WORD: 0xFFFF means unknown; 0x7FFF means
                    // the module is too large for this field and the real figure is the
                    // Extended Size DWORD at 0x1C, in MB with bit 31 reserved; otherwise
                    // bit 15 selects the unit — set is kilobytes, clear is megabytes — and
                    // bits 14:0 carry the magnitude. Measured on this machine: two modules
                    // reporting 0x4000 with bit 15 clear, so 16384 MB each, 32 GiB total.
                    if (sizeField == 0xFFFF) {
                        sizeUnknown = true;
                    } else if (sizeField == 0x7FFF) {
                        if (headerLength >= 0x20) {
                            const quint32 extended =
                                *reinterpret_cast<const quint32 *>(p + 0x1C) & 0x7FFFFFFFu;
                            m.installedBytes += quint64(extended) * 1024ull * 1024ull;
                        } else {
                            sizeUnknown = true;
                        }
                    } else {
                        const quint64 magnitude = sizeField & 0x7FFF;
                        m.installedBytes += (sizeField & 0x8000) ? magnitude * 1024ull
                                                                 : magnitude * 1024ull * 1024ull;
                    }

                    // Memory Type, offset 0x12. Every populated module has to agree: a
                    // machine cannot run two memory types at once, so if they disagree the
                    // table is describing something this cannot name and naming one of
                    // them anyway would be a guess printed as a fact. Codes this does not
                    // recognise contribute nothing rather than counting as disagreement.
                    if (headerLength >= 0x13) {
                        QString name;
                        switch (p[0x12]) {
                        case 0x18: name = QStringLiteral("DDR3"); break;
                        case 0x1A: name = QStringLiteral("DDR4"); break;
                        case 0x22: name = QStringLiteral("DDR5"); break;
                        case 0x23: name = QStringLiteral("LPDDR5"); break;
                        case 0x1E: name = QStringLiteral("LPDDR3"); break;
                        case 0x1F: name = QStringLiteral("LPDDR4"); break;
                        default: break;
                        }
                        if (!name.isEmpty()) {
                            if (type.isEmpty())
                                type = name;
                            else if (type != name)
                                typesDisagree = true;
                        }
                    }

                    // Configured Memory Speed, 7.18.7 at offset 0x20, falling back to the
                    // nominal Speed, 7.18.6 at offset 0x15. Both are MT/s in a WORD, both
                    // read 0 for unknown, and both read 0xFFFF to mean the figure did not
                    // fit and lives in a DWORD added in SMBIOS 3.3 — Extended Speed at 0x54,
                    // Extended Configured Memory Speed at 0x58, bit 31 reserved on each.
                    // That escape is the same shape as the one on Size and is handled for
                    // the same reason: taken literally, 0xFFFF is a plausible-looking 65535
                    // and would print "DDR5-65535". It cannot be reached by anything on sale
                    // — this machine's own modules report 5600 in the WORD and leave both
                    // extended fields zero, measured — but a sentinel read as a magnitude is
                    // the kind of thing that surfaces years later on hardware nobody had.
                    //
                    // The lowest of the populated modules is taken, not the first: a memory
                    // controller clocks every channel together, so a 4800 module beside a
                    // 5600 one runs the pair at 4800, and reporting whichever module the
                    // firmware happened to list first would be reporting a speed the machine
                    // never reaches. Measured here, both modules report a configured 5600 and
                    // the machine runs at 5600.
                    const quint32 length = headerLength;
                    const auto speedAt = [p, length](quint32 wordOffset,
                                                     quint32 extendedOffset) -> quint32 {
                        if (length < wordOffset + 2u)
                            return 0;
                        const quint16 word = *reinterpret_cast<const quint16 *>(p + wordOffset);
                        if (word != 0xFFFF)
                            return word;
                        if (length < extendedOffset + 4u)
                            return 0;
                        return *reinterpret_cast<const quint32 *>(p + extendedOffset) & 0x7FFFFFFFu;
                    };

                    quint32 moduleSpeed = speedAt(0x20, 0x58);
                    if (moduleSpeed == 0)
                        moduleSpeed = speedAt(0x15, 0x54);
                    if (moduleSpeed > 0)
                        speed = speed == 0 ? moduleSpeed : qMin(speed, moduleSpeed);
                }
            }

            // Skip the double-NUL terminated string set that follows each structure.
            p = structEnd;
            while (p + 1 < end && !(p[0] == 0 && p[1] == 0))
                ++p;
            p += 2;
        }

        // One module of unknown size makes the sum an undercount, and an undercount
        // presented as the installed total is worse than not answering: the caller falls
        // back to what the OS can see, which is at least a number that means something.
        if (sizeUnknown)
            m.installedBytes = 0;
        if (typesDisagree)
            type.clear();
        if (!type.isEmpty())
            m.kind = speed > 0 ? QStringLiteral("%1-%2").arg(type).arg(speed) : type;
        return m;
    }();
    return parsed;
}

/// Every display adapter this machine has, each one's name, VRAM, driver version and
/// driver date read together from that adapter's own registry subkey.
///
/// 0.9.10 took the name from the first adapter EnumDisplayDevicesW reported as attached
/// and the driver from the hardcoded subkey "…\\0000", which are not the same adapter on
/// any hybrid laptop: measured on the developer's machine the Hardware block said
/// "Intel(R) UHD Graphics" while the Display block reported NVIDIA's 32.0.15.9620. Reading
/// all four fields out of one key is what makes them agree.
QVector<Facts::Adapter> displayAdapters()
{
    // The PCI ids of whatever is painting the desktop right now. EnumDisplayDevicesW
    // enumerates display *heads*, not adapters — measured here it returns eight entries,
    // four per card, of which exactly one is attached — so it is no use as the adapter
    // list, but it is still the only thing that knows which card is in use, which is worth
    // keeping for a user who has two.
    const QStringList activeIds = [] {
        QStringList ids;
        DISPLAY_DEVICEW device{};
        device.cb = sizeof(device);
        for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &device, 0); ++i) {
            if (device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
                const QString id = QString::fromWCharArray(device.DeviceID).trimmed().toLower();
                if (!id.isEmpty() && !ids.contains(id))
                    ids << id;
            }
            device.cb = sizeof(device);
        }
        return ids;
    }();

    const QString classPath = QStringLiteral(
        "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}");
    QSettings root = hklm(classPath);

    QVector<Facts::Adapter> found;
    const QStringList children = root.childGroups();
    for (const QString &child : children) {
        QSettings entry = hklm(classPath + QLatin1Char('\\') + child);

        // What separates an adapter from the class key's housekeeping, chosen from what is
        // actually in the key rather than from what ought to be: measured, this machine's
        // display class holds "Configuration" and "Properties" beside the three numbered
        // entries, and neither of those carries a DriverDesc, a MatchingDeviceId or any
        // HardwareInformation value. DriverDesc is the test because it is the only one of
        // the three that every real adapter here has — the Intel entry carries no
        // HardwareInformation.qwMemorySize at all, so VRAM cannot be the filter without
        // dropping a card the machine really has.
        Facts::Adapter adapter;
        adapter.name = entry.value(QStringLiteral("DriverDesc")).toString().trimmed();
        if (adapter.name.isEmpty())
            continue;

        // Unchanged from 0.9.10, including the silence: an adapter that does not report
        // its memory says nothing rather than "0 GB".
        const QVariant memory = entry.value(QStringLiteral("HardwareInformation.qwMemorySize"));
        if (memory.isValid()) {
            const quint64 bytes = memory.toULongLong();
            if (bytes > 0)
                adapter.memory = formatBytes(bytes);
        }

        const QString version = entry.value(QStringLiteral("DriverVersion")).toString().trimmed();
        const QString date = entry.value(QStringLiteral("DriverDate")).toString().trimmed();
        if (!version.isEmpty())
            adapter.driver = date.isEmpty() ? version : QStringLiteral("%1 · %2").arg(version, date);

        // MatchingDeviceId is the adapter's PCI id without the revision suffix, so an
        // attached head's DeviceID ("PCI\\VEN_8086&DEV_A78B&SUBSYS_14A71462&REV_04") starts
        // with it. Case is not consistent between the two — measured, the Intel subkey
        // stores the id upper case and the NVIDIA ones lower case — so both sides are
        // folded before the comparison.
        const QString match =
            entry.value(QStringLiteral("MatchingDeviceId")).toString().trimmed().toLower();
        if (!match.isEmpty()) {
            for (const QString &id : activeIds) {
                if (id.startsWith(match)) {
                    adapter.active = true;
                    break;
                }
            }
        }

        // Deduplicate on everything the row will show. Measured, this class key holds the
        // same NVIDIA card twice, as 0000 and 0002, with identical DriverDesc,
        // DriverVersion, DriverDate and qwMemorySize and differing only in the subsystem id
        // inside MatchingDeviceId — subsys_14a71462 against subsys_14a61462. So
        // MatchingDeviceId is exactly the wrong identity here: it separates two entries
        // that are one card. That they are one card was confirmed under
        // HKLM\\SYSTEM\\CurrentControlSet\\Enum\\PCI, where both of those hardware ids
        // resolve to the same device instance, A8BA505BB42DB04800.
        //
        // The rule kept is the one that needs no second lookup and cannot be wrong about
        // what the reader sees: two entries that would draw the same name, the same memory
        // and the same driver are one row, because two identical rows read as a bug. The
        // price is that a machine with two genuinely identical cards collapses to one row.
        // That is rarer than a hybrid laptop, and it under-counts rather than inventing a
        // card the machine does not have.
        bool duplicate = false;
        for (Facts::Adapter &seen : found) {
            if (seen.name == adapter.name && seen.memory == adapter.memory
                && seen.driver == adapter.driver) {
                // Which of the two subkeys carries the id that is on the desktop is an
                // accident of enumeration order, so the mark survives the merge.
                seen.active = seen.active || adapter.active;
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            found.append(adapter);
    }

    // The card in use first, order otherwise as the registry gave them. Both blocks that
    // draw this list draw it in the same order, so the reader can line one against the
    // other, and the answer to "which one am I on" is the top row.
    QVector<Facts::Adapter> ordered;
    ordered.reserve(found.size());
    const QVector<Facts::Adapter> &list = found;
    for (const Facts::Adapter &adapter : list) {
        if (adapter.active)
            ordered.append(adapter);
    }
    for (const Facts::Adapter &adapter : list) {
        if (!adapter.active)
            ordered.append(adapter);
    }
    return ordered;
}

/// NVMe / SSD / HDD for the volume Windows is installed on.
QString systemDriveBus()
{
    const QString root = QDir::toNativeSeparators(QDir::rootPath());   // "C:\"
    if (root.size() < 2)
        return {};

    const QString devicePath = QStringLiteral("\\\\.\\%1:").arg(root.at(0));
    HANDLE h = CreateFileW(reinterpret_cast<const wchar_t *>(devicePath.utf16()), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return {};

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageAdapterProperty;
    query.QueryType = PropertyStandardQuery;

    STORAGE_ADAPTER_DESCRIPTOR descriptor{};
    DWORD returned = 0;
    QString bus;
    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                        &descriptor, sizeof(descriptor), &returned, nullptr)) {
        switch (descriptor.BusType) {
        case BusTypeNvme: bus = QStringLiteral("NVMe"); break;
        case BusTypeSata: bus = QStringLiteral("SATA"); break;
        case BusTypeUsb:  bus = QStringLiteral("USB"); break;
        case BusTypeRAID: bus = QStringLiteral("RAID"); break;
        default: break;
        }
    }
    CloseHandle(h);
    return bus;
}

QDateTime lastLogonTime()
{
    using NetUserGetInfoFn = DWORD(WINAPI *)(LPCWSTR, LPCWSTR, DWORD, LPBYTE *);
    using NetApiBufferFreeFn = DWORD(WINAPI *)(LPVOID);

    // Pinned to System32 rather than left to the default search order, which looks in the
    // directory the executable was started from first. Arbitrium is one portable file that
    // people run out of Downloads, and it always runs elevated: a netapi32.dll next to it would
    // otherwise be loaded into an administrator process. Only these call sites are pinned —
    // a process-wide SetDefaultDllDirectories would also apply to the shell extensions the
    // native file dialogs load, which is not a trade this needs to make.
    HMODULE net = LoadLibraryExW(L"netapi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!net)
        return {};

    QDateTime result;
    auto getInfo = reinterpret_cast<NetUserGetInfoFn>(
        reinterpret_cast<void *>(GetProcAddress(net, "NetUserGetInfo")));
    auto freeBuf = reinterpret_cast<NetApiBufferFreeFn>(
        reinterpret_cast<void *>(GetProcAddress(net, "NetApiBufferFree")));

    if (getInfo && freeBuf) {
        wchar_t user[UNLEN + 1] = {};
        DWORD size = UNLEN + 1;
        if (GetUserNameW(user, &size)) {
            LPBYTE buffer = nullptr;
            if (getInfo(nullptr, user, 2, &buffer) == 0 && buffer) {
                auto *info = reinterpret_cast<USER_INFO_2 *>(buffer);
                if (info->usri2_last_logon > 0)
                    result = QDateTime::fromSecsSinceEpoch(info->usri2_last_logon);
                freeBuf(buffer);
            }
        }
    }
    FreeLibrary(net);
    return result;
}

/// The three markers Windows itself checks before reporting "restart required".
bool rebootPending()
{
    if (hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing"))
            .childGroups().contains(QStringLiteral("RebootPending")))
        return true;

    if (hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update"))
            .childGroups().contains(QStringLiteral("RebootRequired")))
        return true;

    QSettings sm = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\Session Manager"));
    const QVariant pending = sm.value(QStringLiteral("PendingFileRenameOperations"));
    return pending.isValid() && !pending.toStringList().isEmpty();
}

/// Number of policy values in force under a Policies root. Depth-limited because the
/// tree can be deep and this only feeds one informational row.
int countPolicyValues(const QString &root, int depth = 0)
{
    if (depth > 3)
        return 0;
    QSettings s(root, QSettings::NativeFormat);
    int n = s.childKeys().size();
    const QStringList groups = s.childGroups();
    for (const QString &g : groups)
        n += countPolicyValues(root + QLatin1Char('\\') + g, depth + 1);
    return n;
}

/// Primary display mode plus how many panels are attached.
void readDisplays(Facts &f)
{
    int attached = 0;
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &device, 0); ++i) {
        if (device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
            ++attached;
        device.cb = sizeof(device);
    }
    if (attached > 0)
        f.displayCount = Locale::tr(QStringLiteral("sys.ekran")).arg(attached);

    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode)) {
        f.resolution = mode.dmDisplayFrequency > 0
                           ? QStringLiteral("%1×%2 @ %3 Hz").arg(mode.dmPelsWidth)
                                 .arg(mode.dmPelsHeight).arg(mode.dmDisplayFrequency)
                           : QStringLiteral("%1×%2").arg(mode.dmPelsWidth).arg(mode.dmPelsHeight);
        f.colorDepth = QStringLiteral("%1 bit").arg(mode.dmBitsPerPel);
    }

    // 96 dpi is 100%; Windows exposes the scaling factor only as a dpi value.
    using GetDpiForSystemFn = UINT(WINAPI *)();
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        if (auto fn = reinterpret_cast<GetDpiForSystemFn>(
                reinterpret_cast<void *>(GetProcAddress(user32, "GetDpiForSystem")))) {
            const UINT dpi = fn();
            if (dpi > 0)
                f.dpiScale = QStringLiteral("%%1").arg(qRound(dpi * 100.0 / 96.0));
        }
    }

    // The graphics driver is deliberately not read here any more. This function used to
    // take it from the subkey "…\\0000" of the display class, which is whichever adapter
    // Windows numbered first and has nothing to do with the card the Hardware block named:
    // on the developer's laptop that was the NVIDIA entry while the Hardware block showed
    // the Intel card. Adapters and their drivers now come together out of
    // displayAdapters(), and Facts::graphicsDriver is filled from the same entry as
    // Facts::gpu.
}

/// The first operational, non-loopback adapter that holds an IPv4 address.
void readNetwork(Facts &f)
{
    ULONG size = 16 * 1024;
    QByteArray buffer(int(size), Qt::Uninitialized);
    const ULONG flags = GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST;

    ULONG status = GetAdaptersAddresses(
        AF_INET, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    if (status == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(int(size));
        status = GetAdaptersAddresses(
            AF_INET, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    }
    if (status != NO_ERROR)
        return;

    for (auto *a = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()); a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK || a->OperStatus != IfOperStatusUp)
            continue;
        if (!a->FirstUnicastAddress)
            continue;

        auto *sa = reinterpret_cast<sockaddr_in *>(a->FirstUnicastAddress->Address.lpSockaddr);
        wchar_t ip[INET_ADDRSTRLEN] = {};
        if (!InetNtopW(AF_INET, &sa->sin_addr, ip, INET_ADDRSTRLEN))
            continue;

        f.adapter = QString::fromWCharArray(a->FriendlyName).trimmed();
        f.ipv4 = QString::fromWCharArray(ip);

        if (a->PhysicalAddressLength > 0) {
            QStringList octets;
            octets.reserve(int(a->PhysicalAddressLength));
            for (ULONG i = 0; i < a->PhysicalAddressLength; ++i)
                octets << QStringLiteral("%1").arg(a->PhysicalAddress[i], 2, 16, QLatin1Char('0')).toUpper();
            f.macAddress = octets.join(QLatin1Char(':'));
        }

        if (a->TransmitLinkSpeed > 0 && a->TransmitLinkSpeed != ULLONG_MAX) {
            const qreal mbps = a->TransmitLinkSpeed / 1000000.0;
            f.linkSpeed = mbps >= 1000
                              ? QStringLiteral("%1 Gb/s").arg(QLocale().toString(mbps / 1000.0, 'f', 1))
                              : QStringLiteral("%1 Mb/s").arg(qRound(mbps));
        }

        if (a->FirstDnsServerAddress) {
            auto *dns = reinterpret_cast<sockaddr_in *>(a->FirstDnsServerAddress->Address.lpSockaddr);
            wchar_t text[INET_ADDRSTRLEN] = {};
            if (InetNtopW(AF_INET, &dns->sin_addr, text, INET_ADDRSTRLEN))
                f.dnsServer = QString::fromWCharArray(text);
        }

        const QString suffix = a->DnsSuffix ? QString::fromWCharArray(a->DnsSuffix).trimmed() : QString();
        f.domain = suffix.isEmpty() ? Locale::tr(QStringLiteral("sys.calismaGrubu")) : suffix;
        break;
    }
}

void readPower(Facts &f)
{
    SYSTEM_POWER_STATUS power{};
    if (GetSystemPowerStatus(&power)) {
        f.powerSource = power.ACLineStatus == 1   ? Locale::tr(QStringLiteral("sys.sebeke"))
                        : power.ACLineStatus == 0 ? Locale::tr(QStringLiteral("sys.onBattery"))
                                                  : Unknown;

        if (power.BatteryFlag == 128 || power.BatteryLifePercent == 255) {
            f.battery = Locale::tr(QStringLiteral("sys.none"));
        } else {
            const QString state = (power.BatteryFlag & 8)  ? Locale::tr(QStringLiteral("sys.sarjOluyor"))
                                  : power.ACLineStatus == 1 ? Locale::tr(QStringLiteral("sys.batteryFull"))
                                                            : Locale::tr(QStringLiteral("sys.kullanimda"));
            f.battery = QStringLiteral("%%1 · %2").arg(power.BatteryLifePercent).arg(state);
        }
    }

    // The active scheme's friendly name lives under its own GUID key.
    QSettings schemes = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes"));
    const QString guid = schemes.value(QStringLiteral("ActivePowerScheme")).toString();
    if (!guid.isEmpty()) {
        QSettings scheme = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes\\") + guid);
        const QString name = scheme.value(QStringLiteral("FriendlyName")).toString();
        f.powerPlan = name.isEmpty() ? guid : name.section(QLatin1Char(','), -1).trimmed();
    }
}

QString onOff(const QVariant &v, bool invert = false)
{
    if (!v.isValid())
        return Unknown;
    const bool on = invert ? v.toInt() == 0 : v.toInt() != 0;
    return on ? Locale::tr(QStringLiteral("sys.acik")) : Locale::tr(QStringLiteral("sys.kapali"));
}

void readSecurity(Facts &f)
{
    QSettings defender = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection"));
    const QVariant realtime = defender.value(QStringLiteral("DisableRealtimeMonitoring"));
    // On a healthy machine this key is unreadable without elevation, which is tamper
    // protection working as intended — report that instead of guessing "off".
    f.defender = realtime.isValid() ? onOff(realtime, /*invert=*/true) : Locale::tr(QStringLiteral("sys.korumali"));

    QSettings firewall = hklm(QStringLiteral(
        "SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile"));
    f.firewall = onOff(firewall.value(QStringLiteral("EnableFirewall")));

    QSettings explorer = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer"));
    const QString smart = explorer.value(QStringLiteral("SmartScreenEnabled")).toString();
    if (!smart.isEmpty())
        f.smartScreen = smart.compare(QStringLiteral("Off"), Qt::CaseInsensitive) == 0
                            ? Locale::tr(QStringLiteral("sys.kapali"))
                            : Locale::tr(QStringLiteral("sys.acik"));

    QSettings hvci = hklm(QStringLiteral(
        "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity"));
    f.coreIsolation = onOff(hvci.value(QStringLiteral("Enabled")));

    QSettings guard = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\DeviceGuard"));
    f.virtualization = onOff(guard.value(QStringLiteral("EnableVirtualizationBasedSecurity")));
}

void readVolumes(Facts &f)
{
    const QList<QStorageInfo> mounted = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : mounted) {
        if (!volume.isValid() || !volume.isReady() || volume.isReadOnly())
            continue;
        if (volume.bytesTotal() <= 0)
            continue;

        const QString root = volume.rootPath().left(2);
        if (root.size() < 2 || root.at(1) != QLatin1Char(':'))
            continue;

        const qreal total = qreal(volume.bytesTotal());
        const qreal free = qreal(volume.bytesAvailable());

        Facts::Volume entry;
        entry.name = root;
        entry.used = total > 0.0 ? qBound(0.0, (total - free) / total, 1.0) : 0.0;

        const QString fs = QString::fromLatin1(volume.fileSystemType()).toUpper();
        entry.detail = Locale::tr(QStringLiteral("sys.bos"))
                           .arg(formatBytes(quint64(volume.bytesTotal())),
                                formatBytes(quint64(volume.bytesAvailable())));
        if (!fs.isEmpty())
            entry.detail.prepend(fs + QStringLiteral(" · "));

        f.volumes.append(entry);
    }
}

QString formatCount(quint64 n)
{
    return QLocale().toString(qulonglong(n));
}

/// Machine, board and firmware identity. All of it sits in one registry key that
/// Windows populates from SMBIOS at boot.
void readFirmware(Facts &f)
{
    QSettings bios = hklm(QStringLiteral("HARDWARE\\DESCRIPTION\\System\\BIOS"));
    f.manufacturer = orUnknown(bios.value(QStringLiteral("SystemManufacturer")).toString());
    f.model = orUnknown(bios.value(QStringLiteral("SystemProductName")).toString());
    f.biosVendor = orUnknown(bios.value(QStringLiteral("BIOSVendor")).toString());
    f.biosDate = orUnknown(bios.value(QStringLiteral("BIOSReleaseDate")).toString());

    // The SMBIOS version lives in the first two bytes of the raw table header.
    const QByteArray &raw = smbiosTable();
    if (raw.size() >= 8) {
        const uchar major = uchar(raw.at(1));
        const uchar minor = uchar(raw.at(2));
        if (major > 0)
            f.smbios = QStringLiteral("%1.%2").arg(major).arg(minor);
    }

    // A UEFI machine answers this call; a legacy BIOS one fails with
    // ERROR_INVALID_FUNCTION. The dummy GUID is the documented probe.
    SetLastError(ERROR_SUCCESS);
    GetFirmwareEnvironmentVariableW(L"", L"{00000000-0000-0000-0000-000000000000}", nullptr, 0);
    f.bootMode = GetLastError() == ERROR_INVALID_FUNCTION
                     ? Locale::tr(QStringLiteral("sys.bootMode.legacy"))
                     : QStringLiteral("UEFI");
}

void readProcessorDetail(Facts &f)
{
    QSettings cpu = hklm(QStringLiteral("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"));
    f.cpuVendor = orUnknown(cpu.value(QStringLiteral("VendorIdentifier")).toString());

    const int mhz = cpu.value(QStringLiteral("~MHz")).toInt();
    if (mhz > 0)
        f.cpuBaseClock = QStringLiteral("%1 GHz").arg(QLocale().toString(mhz / 1000.0, 'f', 2));

    f.cpuArchitecture = QSysInfo::currentCpuArchitecture();
    f.cpuVirtualization = IsProcessorFeaturePresent(PF_VIRT_FIRMWARE_ENABLED)
                              ? Locale::tr(QStringLiteral("sys.acik"))
                              : Locale::tr(QStringLiteral("sys.kapali"));
}

void readMemoryDetail(Facts &f)
{
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        f.memoryInUse = formatBytes(memory.ullTotalPhys - memory.ullAvailPhys);
        f.memoryFree = formatBytes(memory.ullAvailPhys);

        // The commit limit includes RAM, so the page file is the difference.
        if (memory.ullTotalPageFile > memory.ullTotalPhys)
            f.pageFile = formatBytes(memory.ullTotalPageFile - memory.ullTotalPhys);
        else
            f.pageFile = Locale::tr(QStringLiteral("sys.none"));
    }

    // Populated versus total memory slots, from the SMBIOS type 17 structures this file
    // used to walk a second time here and now parses once, in memoryModules().
    const MemoryModules &modules = memoryModules();
    if (modules.slotCount > 0)
        f.memorySlots = Locale::tr(QStringLiteral("sys.slotsFilled"))
                            .arg(modules.filledCount)
                            .arg(modules.slotCount);
}

/// Counts the values under a Run key.
int countRunEntries(const QString &root)
{
    return QSettings(root, QSettings::NativeFormat).childKeys().size();
}

/// Counts uninstall entries that actually carry a display name.
int countInstalled(const QString &root)
{
    QSettings s(root, QSettings::NativeFormat);
    int n = 0;
    const QStringList children = s.childGroups();
    for (const QString &child : children) {
        QSettings entry(root + QLatin1Char('\\') + child, QSettings::NativeFormat);
        if (!entry.value(QStringLiteral("DisplayName")).toString().trimmed().isEmpty()
            && entry.value(QStringLiteral("SystemComponent")).toInt() == 0)
            ++n;
    }
    return n;
}

void readSoftware(Facts &f)
{
    const int installed =
        countInstalled(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"))
        + countInstalled(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"))
        + countInstalled(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"));
    f.installedPrograms = Locale::tr(QStringLiteral("sys.kayit")).arg(installed);

    const int startup =
        countRunEntries(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"))
        + countRunEntries(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"))
        + countRunEntries(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run"));
    f.startupEntries = Locale::tr(QStringLiteral("sys.giris")).arg(startup);

    QSettings ndp = hklm(QStringLiteral("SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full"));
    const int release = ndp.value(QStringLiteral("Release")).toInt();
    if (release > 0) {
        const QString version = ndp.value(QStringLiteral("Version")).toString();
        f.dotNet = version.isEmpty() ? QStringLiteral("release %1").arg(release) : version;
    }

    QSettings ps = hklm(QStringLiteral("SOFTWARE\\Microsoft\\PowerShell\\3\\PowerShellEngine"));
    f.powerShell = orUnknown(ps.value(QStringLiteral("PowerShellVersion")).toString());

    QSettings choice = hkcu(QStringLiteral(
        "SOFTWARE\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https\\UserChoice"));
    QString progId = choice.value(QStringLiteral("ProgId")).toString();
    if (!progId.isEmpty()) {
        // Turn "MSEdgeHTM" / "ChromeHTML" into something a person recognises.
        QSettings app(QStringLiteral("HKEY_CLASSES_ROOT\\") + progId + QStringLiteral("\\Application"),
                      QSettings::NativeFormat);
        const QString friendly = app.value(QStringLiteral("ApplicationName")).toString();
        f.defaultBrowser = friendly.isEmpty() ? progId : friendly;
    }
}

void readLocale(Facts &f)
{
    QSettings tz = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation"));
    QString zone = tz.value(QStringLiteral("TimeZoneKeyName")).toString().trimmed();
    if (zone.isEmpty())
        zone = tz.value(QStringLiteral("StandardName")).toString().trimmed();

    // Qt, not GetTimeZoneInformation. The hand-rolled version added DaylightBias whether
    // or not daylight time was in effect — the field is filled in regardless, and only the
    // *return value* says which season the machine is in — so half the year was an hour
    // out. It also divided by 60 as an integer, which reads India (bias 330) as UTC+5.
    // Qt asks the zone's full rules and answers in seconds.
    const int secs = QDateTime::currentDateTime().offsetFromUtc();
    const int mins = qAbs(secs) / 60;
    zone = QStringLiteral("%1 (UTC%2%3%4)")
               .arg(zone.isEmpty() ? QStringLiteral("—") : zone,
                    secs < 0 ? QStringLiteral("-") : QStringLiteral("+"))
               .arg(mins / 60)
               .arg(mins % 60 ? QStringLiteral(":%1").arg(mins % 60, 2, 10, QLatin1Char('0'))
                              : QString());
    f.timeZone = orUnknown(zone);

    QSettings w32 = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Services\\W32Time\\Parameters"));
    QString ntp = w32.value(QStringLiteral("NtpServer")).toString();
    f.ntpServer = orUnknown(ntp.section(QLatin1Char(','), 0, 0));

    f.locale = QLocale::system().nativeLanguageName() + QStringLiteral(" · ")
               + QLocale::system().nativeTerritoryName();

    wchar_t layout[KL_NAMELENGTH] = {};
    if (GetKeyboardLayoutNameW(layout))
        f.keyboardLayout = QString::fromWCharArray(layout);
}

void readIdentity(Facts &f)
{
    QSettings cv = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"));
    f.buildBranch = orUnknown(cv.value(QStringLiteral("BuildBranch")).toString());
    f.editionId = orUnknown(cv.value(QStringLiteral("EditionID")).toString());
    f.profilePath = orUnknown(QDir::toNativeSeparators(qEnvironmentVariable("USERPROFILE")));

    wchar_t windows[MAX_PATH] = {};
    if (GetWindowsDirectoryW(windows, MAX_PATH) > 0)
        f.windowsDir = QDir::toNativeSeparators(QString::fromWCharArray(windows));
    f.systemDrive = orUnknown(qEnvironmentVariable("SystemDrive"));

    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw > 0 && vh > 0)
        f.virtualDesktop = QStringLiteral("%1×%2").arg(vw).arg(vh);
}

/// Second pass over the adapter list for the details the first pass does not need.
void readNetworkDetail(Facts &f)
{
    ULONG size = 16 * 1024;
    QByteArray buffer(int(size), Qt::Uninitialized);
    const ULONG flags = GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_INCLUDE_GATEWAYS;

    ULONG status = GetAdaptersAddresses(
        AF_UNSPEC, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    if (status == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(int(size));
        status = GetAdaptersAddresses(
            AF_UNSPEC, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    }
    if (status != NO_ERROR)
        return;

    int adapters = 0;
    bool first = true;
    for (auto *a = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()); a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        ++adapters;
        if (a->OperStatus != IfOperStatusUp || !first)
            continue;

        for (auto *u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET6)
                continue;
            auto *sa = reinterpret_cast<sockaddr_in6 *>(u->Address.lpSockaddr);
            wchar_t text[INET6_ADDRSTRLEN] = {};
            if (InetNtopW(AF_INET6, &sa->sin6_addr, text, INET6_ADDRSTRLEN)) {
                f.ipv6 = QString::fromWCharArray(text);
                break;
            }
        }

        if (a->FirstGatewayAddress) {
            auto *g = a->FirstGatewayAddress->Address.lpSockaddr;
            wchar_t text[INET6_ADDRSTRLEN] = {};
            if (g->sa_family == AF_INET) {
                auto *v4 = reinterpret_cast<sockaddr_in *>(g);
                if (InetNtopW(AF_INET, &v4->sin_addr, text, INET6_ADDRSTRLEN))
                    f.gateway = QString::fromWCharArray(text);
            }
        }
        first = false;
    }
    if (adapters > 0)
        f.adapterCount = Locale::tr(QStringLiteral("sys.bagdastirici")).arg(adapters);
}

#endif // Q_OS_WIN

} // namespace

QString friendlyDateTime(const QDateTime &dt, bool withComma)
{
    if (!dt.isValid())
        return Unknown;

    const QDate today = QDate::currentDate();
    const QString time = dt.time().toString(QStringLiteral("HH:mm"));
    const QString separator = withComma ? QStringLiteral(", ") : QStringLiteral(" ");

    if (dt.date() == today)
        return Locale::tr(QStringLiteral("sys.bugun")) + separator + time;
    if (dt.date() == today.addDays(-1))
        return Locale::tr(QStringLiteral("sys.dun")) + separator + time;
    return dt.toString(QStringLiteral("dd.MM.yyyy")) + separator + time;
}

QString uptimeString()
{
#ifdef Q_OS_WIN
    const qint64 ms = qint64(GetTickCount64());
    return Locale::tr(QStringLiteral("sys.uptimeHm")).arg(ms / 3600000).arg((ms % 3600000) / 60000);
#else
    return Unknown;
#endif
}

LiveCounters liveCounters()
{
    LiveCounters c;
#ifdef Q_OS_WIN
    PERFORMANCE_INFORMATION info{};
    info.cb = sizeof(info);
    if (GetPerformanceInfo(&info, sizeof(info))) {
        c.processes = formatCount(info.ProcessCount);
        c.threads = formatCount(info.ThreadCount);
        c.handles = formatCount(info.HandleCount);
    }

    LASTINPUTINFO last{};
    last.cbSize = sizeof(last);
    if (GetLastInputInfo(&last)) {
        const qint64 idleMs = qint64(GetTickCount64()) - qint64(last.dwTime);
        if (idleMs < 60000)
            c.idle = Locale::tr(QStringLiteral("sys.idleSec")).arg(qMax<qint64>(0, idleMs / 1000));
        else
            c.idle = Locale::tr(QStringLiteral("sys.idleMin")).arg(idleMs / 60000);
    }
#endif
    return c;
}

int buildNumber()
{
    static const int build = [] {
#ifdef Q_OS_WIN
        QSettings cv = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"));
        return cv.value(QStringLiteral("CurrentBuildNumber")).toString().toInt();
#else
        return 0;
#endif
    }();
    return build;
}

Facts collect()
{
    Facts f;

#ifdef Q_OS_WIN
    // ---- Sistem -----------------------------------------------------------
    QSettings cv = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"));

    QString product = cv.value(QStringLiteral("ProductName")).toString();
    const int build = cv.value(QStringLiteral("CurrentBuildNumber")).toString().toInt();
    // The registry still says "Windows 10" on Windows 11; the build number is the truth.
    if (build >= 22000)
        product.replace(QStringLiteral("Windows 10"), QStringLiteral("Windows 11"));
    f.osName = orUnknown(product);

    const QString display = cv.value(QStringLiteral("DisplayVersion")).toString();
    const int ubr = cv.value(QStringLiteral("UBR")).toInt();
    const QString buildString = ubr > 0 ? QStringLiteral("%1.%2").arg(build).arg(ubr)
                                        : QString::number(build);
    f.version = display.isEmpty() ? buildString : QStringLiteral("%1 · %2").arg(display, buildString);

    const uint installSecs = cv.value(QStringLiteral("InstallDate")).toUInt();
    if (installSecs > 0)
        f.installDate = QDateTime::fromSecsSinceEpoch(installSecs).toString(QStringLiteral("dd.MM.yyyy"));

    QSettings secureBoot = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State"));
    const QVariant sb = secureBoot.value(QStringLiteral("UEFISecureBootEnabled"));
    if (sb.isValid())
        f.secureBoot = sb.toInt() ? Locale::tr(QStringLiteral("sys.acik")) : Locale::tr(QStringLiteral("sys.kapali"));

    f.tpm = tpmVersion();

    // ---- Kullanıcı --------------------------------------------------------
    f.elevated = isElevated();
    f.userName = orUnknown(qEnvironmentVariable("USERNAME"));
    f.computerName = orUnknown(QSysInfo::machineHostName().toUpper());

    QSettings identity = hkcu(QStringLiteral("SOFTWARE\\Microsoft\\IdentityCRL\\UserExtendedProperties"));
    f.microsoftAccount = identity.childGroups().isEmpty() ? Locale::tr(QStringLiteral("sys.bagliDegil"))
                                                          : Locale::tr(QStringLiteral("sys.bagli"));

    const QString scope = f.microsoftAccount == Locale::tr(QStringLiteral("sys.bagli")) ? QStringLiteral("Microsoft")
                                                                       : Locale::tr(QStringLiteral("sys.yerel"));
    f.accountType = QStringLiteral("%1 · %2").arg(scope, elevationLabel(f.elevated));

    const int policies = countPolicyValues(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies"))
                         + countPolicyValues(QStringLiteral("HKEY_CURRENT_USER\\Software\\Policies"));
    f.activePolicies = QString::number(policies);

    // ---- Donanım ----------------------------------------------------------
    QSettings cpuKey = hklm(QStringLiteral("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"));
    QString cpuName = cpuKey.value(QStringLiteral("ProcessorNameString")).toString().trimmed();
    cpuName.remove(QStringLiteral("(R)")).remove(QStringLiteral("(TM)")).remove(QStringLiteral("CPU"));
    cpuName = cpuName.simplified();
    if (!cpuName.isEmpty()) {
        const CoreCounts cores = coreCounts();
        f.cpu = cores.physical > 0
                    ? QStringLiteral("%1 · %2C/%3T").arg(cpuName).arg(cores.physical).arg(cores.logical)
                    : cpuName;
    }

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        const MemoryModules &modules = memoryModules();

        // ullTotalPhys is what the OS can address, not what is plugged in. Measured on the
        // developer's laptop: 34,048,495,616 bytes against 34,359,738,368 installed, the
        // 297 MiB difference being what the firmware and the integrated graphics keep. That
        // gap happens to round away here; a machine whose APU carves out 2 GB would have
        // shown "15 GB" beside 16 GB of memory. The type 17 sum is the installed figure.
        //
        // It is only believed when it is at least the visible figure: installed memory
        // cannot be less than what the OS is already using, so a table that says otherwise
        // is not one to report from. That also covers the virtual machine with no type 17
        // at all, where the sum is zero.
        const quint64 visible = mem.ullTotalPhys;
        const quint64 installed = modules.installedBytes >= visible ? modules.installedBytes : visible;

        QString text = formatBytes(installed);
        if (!modules.kind.isEmpty())
            text += QLatin1Char(' ') + modules.kind;

        // …and what the OS can actually see, when that is a different number once rounded.
        // Windows' own System page writes "32.0 GB (31.7 GB usable)" for the same reason.
        //
        // Only when it differs after rounding, because on most machines it does not and a
        // parenthesis that always says the same thing as the figure beside it is noise. But
        // where it does differ the silence is worse: this row is labelled "Bellek", not
        // "Takılı", and the Kullanımda and Boşta rows two blocks away are computed from the
        // visible figure — so an APU that keeps 2 GB would say 16 GB here and account for
        // 14 GB there, with nothing on screen explaining the gap.
        if (formatBytes(installed) != formatBytes(visible))
            text += QStringLiteral(" (%1)").arg(
                Locale::tr(QStringLiteral("sys.kullanilabilir")).arg(formatBytes(visible)));

        f.memory = text;
    }

    f.adapters = displayAdapters();
    if (!f.adapters.isEmpty()) {
        // The one-line summaries the rest of Facts still offers, both taken from the same
        // entry — the adapter driving the desktop, which displayAdapters() sorts first — so
        // that the pair 0.9.10 got from two unrelated places can no longer disagree.
        const Facts::Adapter &primary = f.adapters.constFirst();
        f.gpu = primary.memory.isEmpty()
                    ? primary.name
                    : QStringLiteral("%1 · %2").arg(primary.name, primary.memory);
        if (!primary.driver.isEmpty())
            f.graphicsDriver = primary.driver;
    }

    {
        const QStorageInfo storage = QStorageInfo::root();
        if (storage.isValid()) {
            const QString bus = systemDriveBus();
            const QString size = formatBytes(quint64(storage.bytesTotal()));
            const QString free = Locale::tr(QStringLiteral("sys.bosTek"))
                                     .arg(formatBytes(quint64(storage.bytesAvailable())));
            f.storage = bus.isEmpty() ? QStringLiteral("%1 · %2").arg(size, free)
                                      : QStringLiteral("%1 %2 · %3").arg(bus, size, free);
        }
    }

    QSettings biosKey = hklm(QStringLiteral("HARDWARE\\DESCRIPTION\\System\\BIOS"));
    const QString boardVendor = biosKey.value(QStringLiteral("BaseBoardManufacturer")).toString().trimmed();
    const QString boardName = biosKey.value(QStringLiteral("BaseBoardProduct")).toString().trimmed();
    if (!boardName.isEmpty())
        f.motherboard = boardVendor.isEmpty() ? boardName : QStringLiteral("%1 %2").arg(boardVendor, boardName);

    const QString biosVersion = biosKey.value(QStringLiteral("BIOSVersion")).toString().trimmed();
    if (!biosVersion.isEmpty()) {
        QSettings secure = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State"));
        const bool uefi = secure.value(QStringLiteral("UEFISecureBootEnabled")).isValid();
        f.bios = uefi ? QStringLiteral("%1 · UEFI").arg(biosVersion) : biosVersion;
    }

    // ---- Oturum -----------------------------------------------------------
    const qint64 uptimeMs = qint64(GetTickCount64());
    f.uptime = uptimeString();
    f.lastBoot = friendlyDateTime(QDateTime::currentDateTime().addMSecs(-uptimeMs));
    f.lastLogon = friendlyDateTime(lastLogonTime());
    f.pendingRestart = Locale::tr(rebootPending() ? QStringLiteral("sys.yes") : QStringLiteral("sys.no"));

    readDisplays(f);
    readNetwork(f);
    readPower(f);
    readSecurity(f);
    readVolumes(f);
    readFirmware(f);
    readProcessorDetail(f);
    readMemoryDetail(f);
    readSoftware(f);
    readLocale(f);
    readIdentity(f);
    readNetworkDetail(f);
#else
    f.osName = QSysInfo::prettyProductName();
    f.version = QSysInfo::productVersion();
    f.computerName = QSysInfo::machineHostName().toUpper();
#endif

    // The elevation word is deliberately NOT baked in here: these facts are probed once at
    // startup, while the interface language can change at any moment afterwards. Only the
    // language-independent part is stored; SysInfo::titleBarSummary() adds the rest each
    // time it is asked, so a language switch retranslates it like everything else.
    QStringList summary;
    if (f.osName != Unknown)
        summary << f.osName;
    if (f.version != Unknown)
        summary << f.version.section(QStringLiteral(" · "), -1);
    f.titleBarSummary = summary.join(QStringLiteral(" · "));

    return f;
}

QString elevationLabel(bool elevated)
{
    return Locale::tr(elevated ? QStringLiteral("sys.elevation.admin")
                               : QStringLiteral("sys.elevation.standard"));
}

QString titleBarSummary(const Facts &f)
{
    QStringList parts;
    if (f.titleBarSummary != QStringLiteral("—") && !f.titleBarSummary.isEmpty())
        parts << f.titleBarSummary;
    parts << elevationLabel(f.elevated);
    return parts.join(QStringLiteral(" · "));
}

// ------------------------------------------------------------------- Probe ---

Probe::Probe(QObject *parent)
    : QObject(parent)
{
}

void Probe::start()
{
    if (m_started)
        return;
    m_started = true;

#ifdef Q_OS_WIN
    const QString powershell = QStandardPaths::findExecutable(QStringLiteral("powershell"));
    if (powershell.isEmpty()) {
        Q_EMIT resolved({}, {}, {});
        return;
    }

    // One shot, read-only, no side effects. LicenseStatus 1 == licensed.
    static const char script[] = R"PS(
$ErrorActionPreference='SilentlyContinue'
$lic = Get-CimInstance SoftwareLicensingProduct -Filter "PartialProductKey IS NOT NULL AND ApplicationId='55c92734-d682-4d71-983e-d6ec3f16059f'" | Select-Object -First 1
$rp  = Get-ComputerRestorePoint | Sort-Object CreationTime -Descending | Select-Object -First 1
$qfe = Get-CimInstance Win32_QuickFixEngineering | Sort-Object InstalledOn -Descending | Select-Object -First 1
[pscustomobject]@{
  license = if ($lic) { [int]$lic.LicenseStatus } else { -1 }
  channel = if ($lic) { [string]$lic.ProductKeyChannel } else { '' }
  restore = if ($rp) { $rp.ConvertToDateTime($rp.CreationTime).ToString('o') } else { '' }
  hotfix  = if ($qfe) { [string]$qfe.HotFixID } else { '' }
} | ConvertTo-Json -Compress
)PS";

    auto *process = new QProcess(this);

    // Both signals, with a one-shot guard, the way DeepInfo::Probe does it. QProcess does
    // not emit finished() when the process fails to start, only errorOccurred() — so on a
    // machine where powershell.exe cannot launch, this used to resolve nothing at all and
    // leak the QProcess for the life of the window. And a process that dies emits both,
    // which is the other half of why the guard is here.
    const auto settle = [this, process](bool ok) {
        if (process->property("settled").toBool())
            return;
        process->setProperty("settled", true);
        process->deleteLater();

        if (!ok) {
            Q_EMIT resolved({}, {}, {});
            return;
        }

        const QByteArray out = process->readAllStandardOutput().trimmed();
        m_output = QJsonDocument::fromJson(out).object();
        emitFrom(m_output);
    };

    connect(process, &QProcess::finished, this,
            [settle](int code, QProcess::ExitStatus) { settle(code == 0); });
    connect(process, &QProcess::errorOccurred, this,
            [settle](QProcess::ProcessError) { settle(false); });

    // The same console-encoding preamble DeepInfo::runScript uses, and for the same
    // reason: a fresh console writes in the OEM code page, and one byte of it that is not
    // valid UTF-8 makes Qt reject the whole JSON document.
    const QString preamble =
        QStringLiteral("[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false; "
                       "$OutputEncoding = [Console]::OutputEncoding; ");

    process->start(powershell,
                   {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                    QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                    QStringLiteral("-Command"), preamble + QString::fromUtf8(script)});
#else
    Q_EMIT resolved({}, {}, {});
#endif
}

void Probe::emitFrom(const QJsonObject &o)
{
    QString activation;
    const int license = o.value(QStringLiteral("license")).toInt(-1);
    if (license == 1) {
        const QString channel = o.value(QStringLiteral("channel")).toString();
        activation = channel.compare(QStringLiteral("Retail"), Qt::CaseInsensitive) == 0
                         ? Locale::tr(QStringLiteral("sys.perakendeLisans"))
                     : channel.compare(QStringLiteral("OEM"), Qt::CaseInsensitive) == 0
                         ? Locale::tr(QStringLiteral("sys.oemLisans"))
                         : Locale::tr(QStringLiteral("sys.dijitalLisans"));
    } else if (license >= 0) {
        activation = Locale::tr(QStringLiteral("sys.etkinlestirilmemis"));
    }

    QString restore;
    const QString iso = o.value(QStringLiteral("restore")).toString();
    if (!iso.isEmpty()) {
        const QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
        if (dt.isValid())
            restore = friendlyDateTime(dt, /*withComma=*/true);
    }

    Q_EMIT resolved(activation, restore, o.value(QStringLiteral("hotfix")).toString());
}

void Probe::retranslate()
{
    // Nothing to replay before the run lands, or if it failed — and a failure must not
    // overwrite anything, so m_output stays empty on that path.
    if (!m_output.isEmpty())
        emitFrom(m_output);
}

} // namespace SysInfo
