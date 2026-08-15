#include "sysinfo.h"

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
#endif

namespace SysInfo {
namespace {

const QString Unknown = QStringLiteral("—");

QSettings hklm(const QString &path)
{
    return QSettings(QStringLiteral("HKEY_LOCAL_MACHINE\\") + path, QSettings::NativeFormat);
}

QSettings hkcu(const QString &path)
{
    return QSettings(QStringLiteral("HKEY_CURRENT_USER\\") + path, QSettings::NativeFormat);
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
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
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

    HMODULE tbs = LoadLibraryW(L"tbs.dll");
    if (!tbs)
        return QStringLiteral("Yok");

    QString result = QStringLiteral("Yok");
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

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    c.logical = int(si.dwNumberOfProcessors);

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

/// Memory type and speed from SMBIOS table type 17 (Memory Device).
QString memoryKind()
{
    // GetSystemFirmwareTable's provider signature 'RSMB', spelled out so it is not a
    // multi-character literal with implementation-defined ordering.
    constexpr DWORD RawSmbios = 0x52534D42;

    const DWORD size = GetSystemFirmwareTable(RawSmbios, 0, nullptr, 0);
    if (size == 0)
        return {};

    QByteArray raw(int(size), Qt::Uninitialized);
    if (GetSystemFirmwareTable(RawSmbios, 0, raw.data(), size) != size)
        return {};
    if (raw.size() < 8)
        return {};

    // RawSMBIOSData header: 4 bytes of version info, then DWORD length, then the table.
    const quint32 tableLength = *reinterpret_cast<const quint32 *>(raw.constData() + 4);
    const uchar *p = reinterpret_cast<const uchar *>(raw.constData()) + 8;
    const uchar *end = p + qMin<quint32>(tableLength, quint32(raw.size() - 8));

    QString type;
    int speed = 0;

    while (p + 4 <= end) {
        const uchar structType = p[0];
        const uchar headerLength = p[1];
        if (headerLength < 4)
            break;
        const uchar *structEnd = p + headerLength;

        if (structType == 17 && headerLength >= 0x17) {
            const quint16 sizeField = *reinterpret_cast<const quint16 *>(p + 0x0C);
            if (sizeField != 0) {   // populated slot
                if (type.isEmpty()) {
                    switch (p[0x12]) {
                    case 0x18: type = QStringLiteral("DDR3"); break;
                    case 0x1A: type = QStringLiteral("DDR4"); break;
                    case 0x22: type = QStringLiteral("DDR5"); break;
                    case 0x23: type = QStringLiteral("LPDDR5"); break;
                    case 0x1E: type = QStringLiteral("LPDDR3"); break;
                    case 0x1F: type = QStringLiteral("LPDDR4"); break;
                    default: break;
                    }
                }
                if (speed == 0) {
                    if (headerLength >= 0x22) {
                        const quint16 configured = *reinterpret_cast<const quint16 *>(p + 0x20);
                        if (configured > 0)
                            speed = configured;
                    }
                    if (speed == 0) {
                        const quint16 nominal = *reinterpret_cast<const quint16 *>(p + 0x15);
                        if (nominal > 0)
                            speed = nominal;
                    }
                }
            }
        }

        // Skip the double-NUL terminated string set that follows each structure.
        p = structEnd;
        while (p + 1 < end && !(p[0] == 0 && p[1] == 0))
            ++p;
        p += 2;
    }

    if (type.isEmpty())
        return {};
    return speed > 0 ? QStringLiteral("%1-%2").arg(type).arg(speed) : type;
}

QString gpuDescription()
{
    DISPLAY_DEVICEW dd{};
    dd.cb = sizeof(dd);
    QString name;
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            name = QString::fromWCharArray(dd.DeviceString).trimmed();
            break;
        }
        dd.cb = sizeof(dd);
    }
    if (name.isEmpty())
        return {};

    // VRAM lives beside the driver entry for the display class.
    const QString classPath = QStringLiteral(
        "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}");
    QSettings root = hklm(classPath);
    const QStringList children = root.childGroups();
    for (const QString &child : children) {
        QSettings entry = hklm(classPath + QLatin1Char('\\') + child);
        if (entry.value(QStringLiteral("DriverDesc")).toString().trimmed() != name)
            continue;
        const QVariant qw = entry.value(QStringLiteral("HardwareInformation.qwMemorySize"));
        if (qw.isValid()) {
            const quint64 bytes = qw.toULongLong();
            if (bytes > 0)
                return QStringLiteral("%1 · %2").arg(name, formatBytes(bytes));
        }
    }
    return name;
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

    HMODULE net = LoadLibraryW(L"netapi32.dll");
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
        f.displayCount = QStringLiteral("%1 ekran").arg(attached);

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

    QSettings gpu = hklm(QStringLiteral(
        "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000"));
    const QString version = gpu.value(QStringLiteral("DriverVersion")).toString();
    const QString date = gpu.value(QStringLiteral("DriverDate")).toString();
    if (!version.isEmpty())
        f.graphicsDriver = date.isEmpty() ? version
                                          : QStringLiteral("%1 · %2").arg(version, date);
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
        f.domain = suffix.isEmpty() ? QStringLiteral("Çalışma grubu") : suffix;
        break;
    }
}

void readPower(Facts &f)
{
    SYSTEM_POWER_STATUS power{};
    if (GetSystemPowerStatus(&power)) {
        f.powerSource = power.ACLineStatus == 1   ? QStringLiteral("Şebeke")
                        : power.ACLineStatus == 0 ? QStringLiteral("Pil")
                                                  : Unknown;

        if (power.BatteryFlag == 128 || power.BatteryLifePercent == 255) {
            f.battery = QStringLiteral("Yok");
        } else {
            const QString state = (power.BatteryFlag & 8)  ? QStringLiteral("şarj oluyor")
                                  : power.ACLineStatus == 1 ? QStringLiteral("dolu")
                                                            : QStringLiteral("kullanımda");
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
    return on ? QStringLiteral("Açık") : QStringLiteral("Kapalı");
}

void readSecurity(Facts &f)
{
    QSettings defender = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection"));
    const QVariant realtime = defender.value(QStringLiteral("DisableRealtimeMonitoring"));
    // On a healthy machine this key is unreadable without elevation, which is tamper
    // protection working as intended — report that instead of guessing "off".
    f.defender = realtime.isValid() ? onOff(realtime, /*invert=*/true) : QStringLiteral("Korumalı");

    QSettings firewall = hklm(QStringLiteral(
        "SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile"));
    f.firewall = onOff(firewall.value(QStringLiteral("EnableFirewall")));

    QSettings explorer = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer"));
    const QString smart = explorer.value(QStringLiteral("SmartScreenEnabled")).toString();
    if (!smart.isEmpty())
        f.smartScreen = smart.compare(QStringLiteral("Off"), Qt::CaseInsensitive) == 0
                            ? QStringLiteral("Kapalı")
                            : QStringLiteral("Açık");

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

        f.volumes.append({root, QStringLiteral("%1 · %2 boş")
                                    .arg(formatBytes(quint64(volume.bytesTotal())),
                                         formatBytes(quint64(volume.bytesAvailable())))});
    }
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
        return QStringLiteral("Bugün") + separator + time;
    if (dt.date() == today.addDays(-1))
        return QStringLiteral("Dün") + separator + time;
    return dt.toString(QStringLiteral("dd.MM.yyyy")) + separator + time;
}

QString uptimeString()
{
#ifdef Q_OS_WIN
    const qint64 ms = qint64(GetTickCount64());
    return QStringLiteral("%1 sa %2 dk").arg(ms / 3600000).arg((ms % 3600000) / 60000);
#else
    return Unknown;
#endif
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
        f.secureBoot = sb.toInt() ? QStringLiteral("Açık") : QStringLiteral("Kapalı");

    f.tpm = tpmVersion();

    // ---- Kullanıcı --------------------------------------------------------
    f.elevated = isElevated();
    f.userName = orUnknown(qEnvironmentVariable("USERNAME"));
    f.computerName = orUnknown(QSysInfo::machineHostName().toUpper());

    QSettings identity = hkcu(QStringLiteral("SOFTWARE\\Microsoft\\IdentityCRL\\UserExtendedProperties"));
    f.microsoftAccount = identity.childGroups().isEmpty() ? QStringLiteral("Bağlı değil")
                                                          : QStringLiteral("Bağlı");

    const QString scope = f.microsoftAccount == QStringLiteral("Bağlı") ? QStringLiteral("Microsoft")
                                                                       : QStringLiteral("Yerel");
    f.accountType = QStringLiteral("%1 · %2").arg(scope,
                                                  f.elevated ? QStringLiteral("Yönetici")
                                                             : QStringLiteral("Standart"));

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
        const QString kind = memoryKind();
        const QString total = formatBytes(mem.ullTotalPhys);
        f.memory = kind.isEmpty() ? total : QStringLiteral("%1 %2").arg(total, kind);
    }

    f.gpu = orUnknown(gpuDescription());

    {
        const QStorageInfo storage = QStorageInfo::root();
        if (storage.isValid()) {
            const QString bus = systemDriveBus();
            const QString size = formatBytes(quint64(storage.bytesTotal()));
            const QString free = QStringLiteral("%1 boş").arg(formatBytes(quint64(storage.bytesAvailable())));
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
    f.pendingRestart = rebootPending() ? QStringLiteral("Var") : QStringLiteral("Yok");

    readDisplays(f);
    readNetwork(f);
    readPower(f);
    readSecurity(f);
    readVolumes(f);
#else
    f.osName = QSysInfo::prettyProductName();
    f.version = QSysInfo::productVersion();
    f.computerName = QSysInfo::machineHostName().toUpper();
#endif

    QStringList summary;
    if (f.osName != Unknown)
        summary << f.osName;
    if (f.version != Unknown)
        summary << f.version.section(QStringLiteral(" · "), -1);
    summary << (f.elevated ? QStringLiteral("Yönetici") : QStringLiteral("Standart"));
    f.titleBarSummary = summary.join(QStringLiteral(" · "));

    return f;
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
    connect(process, &QProcess::finished, this, [this, process](int, QProcess::ExitStatus) {
        process->deleteLater();

        const QByteArray out = process->readAllStandardOutput().trimmed();
        const QJsonObject o = QJsonDocument::fromJson(out).object();

        QString activation;
        const int license = o.value(QStringLiteral("license")).toInt(-1);
        if (license == 1) {
            const QString channel = o.value(QStringLiteral("channel")).toString();
            activation = channel.compare(QStringLiteral("Retail"), Qt::CaseInsensitive) == 0
                             ? QStringLiteral("Perakende lisans")
                         : channel.compare(QStringLiteral("OEM"), Qt::CaseInsensitive) == 0
                             ? QStringLiteral("OEM lisans")
                             : QStringLiteral("Dijital lisans");
        } else if (license >= 0) {
            activation = QStringLiteral("Etkinleştirilmemiş");
        }

        QString restore;
        const QString iso = o.value(QStringLiteral("restore")).toString();
        if (!iso.isEmpty()) {
            const QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
            if (dt.isValid())
                restore = friendlyDateTime(dt, /*withComma=*/true);
        }

        Q_EMIT resolved(activation, restore, o.value(QStringLiteral("hotfix")).toString());
    });

    process->start(powershell,
                   {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                    QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                    QStringLiteral("-Command"), QString::fromLatin1(script)});
#else
    Q_EMIT resolved({}, {}, {});
#endif
}

} // namespace SysInfo
