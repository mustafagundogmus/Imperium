#include "services.h"
#include "i18n.h"

#include <QCollator>
#include <QHash>
#include <QSet>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shlwapi.h>
#endif

#include <algorithm>

namespace Services {
namespace {

#ifdef Q_OS_WIN

constexpr DWORD ServiceWin32 = 0x00000010 | 0x00000020;   // OWN_PROCESS | SHARE_PROCESS

// SERVICE_USERSERVICE_INSTANCE. A per-user service exists twice in the registry: a
// template (CDPUserSvc, cbdhsvc, ...) whose Start value persists and is inherited, and one
// instance per logon session (CDPUserSvc_81365) that is created at sign-in and destroyed
// at sign-out. Both satisfy the Win32 mask, so the list carried 24 rows named after a
// session LUID that vanish on the next reboot and cannot usefully be changed. The
// template stays — it is the only thing that can actually be switched — and the instances
// go. SERVICE_USER_SERVICE (0x40) is deliberately not part of this test: it is set on the
// template too.
constexpr DWORD ServiceUserInstance = 0x00000080;

const wchar_t *wide(const QString &s)
{
    return reinterpret_cast<const wchar_t *>(s.utf16());
}

QString readString(HKEY key, const wchar_t *name)
{
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS)
        return {};
    if (type != REG_SZ && type != REG_EXPAND_SZ)
        return {};

    QByteArray buffer(int(size), Qt::Uninitialized);
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<BYTE *>(buffer.data()), &size) != ERROR_SUCCESS)
        return {};

    // With an explicit length: nothing obliges a registry string to be null-terminated,
    // and letting fromWCharArray look for the terminator itself reads past the buffer on
    // the ones that are not.
    int chars = int(size / sizeof(wchar_t));
    const auto *text = reinterpret_cast<const wchar_t *>(buffer.constData());
    while (chars > 0 && text[chars - 1] == L'\0')
        --chars;
    return QString::fromWCharArray(text, chars).trimmed();
}

bool readDword(HKEY key, const wchar_t *name, DWORD *out)
{
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    return RegQueryValueExW(key, name, nullptr, &type,
                            reinterpret_cast<BYTE *>(out), &size) == ERROR_SUCCESS
           && type == REG_DWORD;
}

/// The services this machine is not expected to boot, log in or elevate without. Kept
/// short on purpose: everything these depend on is locked with them by the closure
/// below, which is where the long tail of "why did my machine stop working" lives.
const char *const CoreServices[] = {
    "RpcSs", "DcomLaunch", "RpcEptMapper",      // COM/RPC — nearly everything needs these
    "LSM", "SamSs", "ProfSvc", "UserManager",   // sessions, accounts, profiles
    "AppInfo",                                  // the UAC elevation service itself
    "PlugPlay", "Power", "DeviceInstall",       // devices and power
    "BFE", "nsi", "Dhcp", "Dnscache",           // the network stack's own base
    "CryptSvc", "EventLog", "Schedule",         // certificates, logging, scheduled tasks
    "gpsvc", "Winmgmt", "StateRepository",      // policy, WMI, the app model's database
    "CoreMessagingRegistrar", "SystemEventsBroker",
    "TrustedInstaller", "msiserver",            // servicing and installers
    "ShellHWDetection", "Themes", "AudioEndpointBuilder",
};

/// Services you may well want to disable, each of which has something that stops working
/// when you do. Locking these would be paternalistic; letting them go quietly would not be
/// honest — a disabled Windows Update or firewall should say so on the row.
///
/// The consequence itself is not here. It used to be, as a Turkish sentence beside each
/// name, which meant the other nine languages showed a Turkish warning. Each row's
/// sentence now lives in resources/data/i18n.json under `svc.risk.<ServiceKey>` and is
/// looked up when the row is drawn, so it follows the interface language like everything
/// else. The key is derived from the name below and cannot drift from it.
const char *const RiskyServices[] = {
    "WinDefend",
    "MpsSvc",
    "wscsvc",
    "SecurityHealthService",
    "Sense",
    "wuauserv",
    "UsoSvc",
    "BITS",
    "DoSvc",
    "Spooler",
    "Audiosrv",
    "WlanSvc",
    "Netman",
    "netprofm",
    "WSearch",
    "SysMain",
    "VSS",
    "swprv",
    "wbengine",
    "WerSvc",
    "LanmanServer",
    "LanmanWorkstation",
    "WpnService",
    "CDPSvc",
    "TabletInputService",
    "seclogon",
    "Themes",
    "FontCache",
};

QStringList multiString(HKEY key, const wchar_t *name)
{
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS)
        return {};
    if (type != REG_MULTI_SZ || size < sizeof(wchar_t))
        return {};

    QByteArray buffer(int(size), Qt::Uninitialized);
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<BYTE *>(buffer.data()), &size) != ERROR_SUCCESS)
        return {};

    QStringList list;
    const auto *p = reinterpret_cast<const wchar_t *>(buffer.constData());
    const auto *end = p + size / sizeof(wchar_t);
    while (p < end && *p) {
        // Bounded, for the same reason readString above is: nothing obliges a registry
        // string to be null-terminated, and letting fromWCharArray look for the terminator
        // itself reads past the block. QByteArray's own trailing NUL is one byte; a
        // wchar_t needs two.
        const auto *stop = std::find(p, end, L'\0');
        const QString entry = QString::fromWCharArray(p, int(stop - p));
        if (!entry.isEmpty())
            list << entry;
        p = stop + 1;   // may reach end + 1; the loop test above catches it
    }
    return list;
}

/// Windows stores service names and descriptions as "@C:\path\to.dll,-42": a pointer
/// into a resource table, in the system's language. Anything else is already text.
QString resolve(const QString &raw)
{
    if (!raw.startsWith(QLatin1Char('@')))
        return raw;

    wchar_t out[1024] = {};
    if (SUCCEEDED(SHLoadIndirectString(wide(raw), out, ARRAYSIZE(out), nullptr)))
        return QString::fromWCharArray(out).trimmed();
    return {};   // an unresolvable pointer is worse than nothing
}

#endif   // Q_OS_WIN

} // namespace

QVector<Info> enumerate()
{
    QVector<Info> services;

#ifdef Q_OS_WIN
    QHash<QString, QStringList> dependencies;

    HKEY root = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0,
                      KEY_READ, &root) != ERROR_SUCCESS)
        return services;

    for (DWORD index = 0;; ++index) {
        wchar_t name[256] = {};
        DWORD length = ARRAYSIZE(name);
        const LONG status = RegEnumKeyExW(root, index, name, &length,
                                          nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS)
            break;
        if (status != ERROR_SUCCESS)
            continue;

        HKEY key = nullptr;
        if (RegOpenKeyExW(root, name, 0, KEY_READ, &key) != ERROR_SUCCESS)
            continue;

        DWORD type = 0;
        DWORD start = 0;
        const bool haveType = readDword(key, L"Type", &type);
        const bool haveStart = readDword(key, L"Start", &start);

        // Drivers have their own start semantics (boot, system) and no business in a
        // list that offers "automatic, manual, disabled".
        if (!haveType || !haveStart || !(type & ServiceWin32) || (type & ServiceUserInstance)
            || start < 2 || start > 4) {
            RegCloseKey(key);
            continue;
        }

        Info info;
        info.key = QString::fromWCharArray(name, int(length));
        info.displayName = resolve(readString(key, L"DisplayName"));
        info.description = resolve(readString(key, L"Description"));
        info.start = int(start);

        DWORD delayed = 0;
        info.delayed = readDword(key, L"DelayedAutostart", &delayed) && delayed != 0;

        dependencies.insert(info.key.toLower(), multiString(key, L"DependOnService"));

        RegCloseKey(key);

        if (info.displayName.isEmpty())
            info.displayName = info.key;
        services.append(info);
    }

    RegCloseKey(root);

    // --- what must not be touched -------------------------------------------
    //
    // Seed with the core list, then walk DependOnService until nothing new turns up: a
    // service the seed cannot start without is every bit as load-bearing as the seed.
    QSet<QString> locked;
    QStringList frontier;
    for (const char *core : CoreServices)
        frontier << QString::fromLatin1(core).toLower();

    while (!frontier.isEmpty()) {
        const QString current = frontier.takeLast();
        if (locked.contains(current))
            continue;
        locked.insert(current);
        for (const QString &needed : dependencies.value(current))
            frontier << needed.toLower();
    }

    // --- which are running --------------------------------------------------
    QSet<QString> running;
    if (SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE)) {
        DWORD needed = 0;
        DWORD count = 0;
        DWORD resume = 0;
        EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE,
                              nullptr, 0, &needed, &count, &resume, nullptr);
        if (needed > 0) {
            QByteArray buffer(int(needed), Qt::Uninitialized);
            resume = 0;
            if (EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE,
                                      reinterpret_cast<LPBYTE>(buffer.data()), needed,
                                      &needed, &count, &resume, nullptr)) {
                const auto *entries =
                    reinterpret_cast<const ENUM_SERVICE_STATUS_PROCESSW *>(buffer.constData());
                for (DWORD i = 0; i < count; ++i)
                    running.insert(QString::fromWCharArray(entries[i].lpServiceName).toLower());
            }
        }
        CloseServiceHandle(scm);
    }

    // The consequence is looked up, not stored: it used to be a Turkish literal in the
    // table above, copied verbatim into the row and shown untranslated in the other nine
    // languages. The key is derived from the service name so the table cannot drift from
    // the strings — svc.risk.<ServiceKey>, one per row, all ten languages.
    QHash<QString, QString> risky;
    for (const char *entry : RiskyServices) {
        const QString key = QString::fromLatin1(entry);
        risky.insert(key.toLower(), QStringLiteral("svc.risk.") + key);
    }

    for (Info &info : services) {
        const QString id = info.key.toLower();
        info.running = running.contains(id);
        info.riskNoteKey = risky.value(id);
        if (locked.contains(id)) {
            info.locked = true;
            info.lockReason = Locale::tr(QStringLiteral("svc.lockReason"));
        }
    }

    // Locale-aware, because the display names come back in the system's language.
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(services.begin(), services.end(), [&collator](const Info &a, const Info &b) {
        return collator.compare(a.displayName, b.displayName) < 0;
    });
#endif

    return services;
}

} // namespace Services
