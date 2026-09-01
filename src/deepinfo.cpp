#include "deepinfo.h"

#include "i18n.h"
#include "registry.h"
#include "sysinfo.h"
#include "winpaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>

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
#  include <psapi.h>
#endif

namespace DeepInfo {
namespace {

// Unknown lives in the header now — Facts defaults every field to it, so it has to be
// visible to anyone who declares one.

/// Not named tr(): inside a QObject member, unqualified tr() resolves to QObject::tr
/// long before it reaches this namespace, and QObject::tr hands back its own argument —
/// so every lookup here would have rendered the raw key on screen.
QString word(const char *key)
{
    return Locale::tr(QString::fromLatin1(key));
}

QString onOff(bool on)
{
    return word(on ? "sys.acik" : "sys.kapali");
}

#ifdef Q_OS_WIN

QSettings hklm(const QString &path)
{
    return Registry::openKey(Registry::Hive::HKLM, path);
}

QSettings hkcu(const QString &path)
{
    return Registry::openKey(Registry::Hive::HKCU, path);
}

/// True when the HKLM key at \a path is there, whatever it holds.
///
/// Deliberately not QSettings: Qt opens a key for reading through createOrOpenKey, so
/// asking QSettings about a key that does not exist creates it. Several of the flags this
/// answers for are pending-restart markers — writing one while checking for it would be
/// telling the user to reboot because we asked whether they had to.
bool hklmKeyExists(const char *path)
{
    HKEY key = nullptr;
    const QString wide = QString::fromLatin1(path);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, reinterpret_cast<const wchar_t *>(wide.utf16()),
                      0, KEY_READ, &key)
        != ERROR_SUCCESS)
        return false;
    RegCloseKey(key);
    return true;
}

/// Whether this machine has a battery at all, which is not the same question as whether
/// its capacity registers can be read.
bool hasBattery()
{
    SYSTEM_POWER_STATUS status{};
    if (!GetSystemPowerStatus(&status))
        return false;
    // 128 is "no system battery"; 255 is "unknown", which is not a denial.
    return status.BatteryFlag != 128;
}

/// A registry DWORD, or \a fallback when the value is missing or not a number.
int dword(QSettings &key, const char *name, int fallback)
{
    const QVariant v = key.value(QString::fromLatin1(name));
    if (!v.isValid())
        return fallback;
    bool ok = false;
    const int n = v.toInt(&ok);
    return ok ? n : fallback;
}

#endif   // Q_OS_WIN

/// A count from the JSON, grouped for the locale. Worth showing even when it is zero —
/// "0 devre dışı" is an answer, an empty row is not.
QString count(const QJsonObject &o, const char *name)
{
    const QJsonValue v = o.value(QString::fromLatin1(name));
    if (!v.isDouble())
        return Unknown;
    return QLocale().toString(qint64(v.toDouble()));
}

#ifdef Q_OS_WIN

// --- system integrity -------------------------------------------------------

/// The bug-check code a kernel minidump was written for.
///
/// A DUMP_HEADER64 opens with 'PAGE' / 'DU64' and carries the code at offset 0x38. The
/// name is not localised on purpose: KERNEL_SECURITY_CHECK_FAILURE is the string every
/// search result and every vendor knowledge-base article is written against, and
/// translating it would only make it unsearchable.
QString bugCheckName(quint32 code)
{
    switch (code) {
    case 0x0000000A: return QStringLiteral("IRQL_NOT_LESS_OR_EQUAL");
    case 0x00000018: return QStringLiteral("REFERENCE_BY_POINTER");
    case 0x00000019: return QStringLiteral("BAD_POOL_HEADER");
    case 0x0000001A: return QStringLiteral("MEMORY_MANAGEMENT");
    case 0x0000001E: return QStringLiteral("KMODE_EXCEPTION_NOT_HANDLED");
    case 0x0000003B: return QStringLiteral("SYSTEM_SERVICE_EXCEPTION");
    case 0x0000004E: return QStringLiteral("PFN_LIST_CORRUPT");
    case 0x00000050: return QStringLiteral("PAGE_FAULT_IN_NONPAGED_AREA");
    case 0x0000007E: return QStringLiteral("SYSTEM_THREAD_EXCEPTION_NOT_HANDLED");
    case 0x0000009F: return QStringLiteral("DRIVER_POWER_STATE_FAILURE");
    case 0x000000A0: return QStringLiteral("INTERNAL_POWER_ERROR");
    case 0x000000BE: return QStringLiteral("ATTEMPTED_WRITE_TO_READONLY_MEMORY");
    case 0x000000C2: return QStringLiteral("BAD_POOL_CALLER");
    case 0x000000D1: return QStringLiteral("DRIVER_IRQL_NOT_LESS_OR_EQUAL");
    case 0x000000EF: return QStringLiteral("CRITICAL_PROCESS_DIED");
    case 0x000000F4: return QStringLiteral("CRITICAL_OBJECT_TERMINATION");
    case 0x00000101: return QStringLiteral("CLOCK_WATCHDOG_TIMEOUT");
    case 0x00000109: return QStringLiteral("CRITICAL_STRUCTURE_CORRUPTION");
    case 0x00000113: return QStringLiteral("VIDEO_DXGKRNL_FATAL_ERROR");
    case 0x00000116: return QStringLiteral("VIDEO_TDR_FAILURE");
    case 0x00000124: return QStringLiteral("WHEA_UNCORRECTABLE_ERROR");
    case 0x00000133: return QStringLiteral("DPC_WATCHDOG_VIOLATION");
    case 0x00000139: return QStringLiteral("KERNEL_SECURITY_CHECK_FAILURE");
    case 0x0000014F: return QStringLiteral("PDC_WATCHDOG_TIMEOUT");
    case 0x00000154: return QStringLiteral("UNEXPECTED_STORE_EXCEPTION");
    default: break;
    }
    return QStringLiteral("0x%1").arg(code, 8, 16, QLatin1Char('0')).toUpper();
}

quint32 bugCheckOf(const QString &dumpPath)
{
    QFile file(dumpPath);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    const QByteArray head = file.read(0x40);

    // A kernel dump opens with 'PAGE' and then either 'DU64' or 'DUMP'; a user-mode
    // minidump opens with 'MDMP' and has none of this layout. The two kernel headers put
    // the bug-check code in different places, because the pointer-sized fields ahead of
    // it are half the width in the 32-bit one.
    int offset = 0;
    if (head.startsWith("PAGEDU64"))
        offset = 0x38;
    else if (head.startsWith("PAGEDUMP"))
        offset = 0x28;
    else
        return 0;

    if (head.size() < offset + int(sizeof(quint32)))
        return 0;
    quint32 code = 0;
    memcpy(&code, head.constData() + offset, sizeof(code));
    return code;
}

/// Every pending-restart flag Windows sets, and which one is actually set. Naming the
/// reason is the point: "Var" tells you to reboot, "Bileşen deposu" tells you why.
QString pendingRestartReason()
{
    struct Flag
    {
        const char *root;
        const char *value;   ///< nullptr = the key's mere existence is the flag
        const char *key;     ///< i18n key for what it means
    };
    static const Flag flags[] = {
        {"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending",
         nullptr, "deep.restart.cbs"},
        {"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired",
         nullptr, "deep.restart.update"},
        {"SYSTEM\\CurrentControlSet\\Control\\Session Manager", "PendingFileRenameOperations",
         "deep.restart.fileRename"},
        {"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\PackagesPending",
         nullptr, "deep.restart.packages"},
        // Both are subkeys of Netlogon, not values in it — a domain join or a pending SPN
        // change leaves a key called JoinDomain or AvoidSpnSet behind, and reading a value
        // of that name out of the parent answered "no" on every machine.
        {"SYSTEM\\CurrentControlSet\\Services\\Netlogon\\JoinDomain", nullptr, "deep.restart.domain"},
        {"SYSTEM\\CurrentControlSet\\Services\\Netlogon\\AvoidSpnSet", nullptr, "deep.restart.domain"},
    };

    QStringList reasons;
    for (const Flag &flag : flags) {
        bool set = false;
        if (flag.value) {
            QSettings key = hklm(QString::fromLatin1(flag.root));
            set = !key.value(QString::fromLatin1(flag.value)).toStringList().isEmpty();
        } else {
            // The key's existence *is* the flag, and the two that matter most —
            // Component Based Servicing\RebootPending and Auto Update\RebootRequired —
            // are created by Windows completely empty. Asking QSettings whether the key
            // holds anything therefore answered "no" whether or not it was there, so the
            // most common pending restart on any machine was the one this never saw. It
            // also has to be asked with RegOpenKeyEx rather than by building a QSettings
            // on the path: QSettings creates a key it cannot open, so merely looking
            // would have written the flag it was looking for.
            set = hklmKeyExists(flag.root);
        }
        // Two flags can name the same reason; say it once.
        if (set && !reasons.contains(word(flag.key)))
            reasons << word(flag.key);
    }

    if (reasons.isEmpty())
        return word("sys.no");
    return reasons.join(QStringLiteral(" · "));
}

// --- connection -------------------------------------------------------------

/// How many TCP connections are actually established right now. Listening sockets are
/// not counted: every machine has dozens and they say nothing about what it is doing.
QString establishedConnections()
{
    ULONG size = 0;
    if (GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0)
        != ERROR_INSUFFICIENT_BUFFER)
        return Unknown;

    QByteArray buffer(int(size), Qt::Uninitialized);
    if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0)
        != NO_ERROR)
        return Unknown;

    const auto *table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID *>(buffer.constData());
    int established = 0;
    for (DWORD i = 0; i < table->dwNumEntries; ++i)
        if (table->table[i].dwState == MIB_TCP_STATE_ESTAB)
            ++established;
    return QLocale().toString(established);
}

/// Whether the adapter carrying the default route takes its address from DHCP.
QString dhcpState()
{
    ULONG size = 0;
    // GAA_FLAG_INCLUDE_GATEWAYS is not optional here: FirstGatewayAddress is only filled
    // in when it is asked for, and the loop below rejects every adapter that has no
    // gateway. Without it the answer was not "no DHCP", it was no adapter at all, and the
    // row read "—" on every machine. sysinfo.cpp passes it for the same structure.
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                        | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW)
        return Unknown;

    QByteArray buffer(int(size), Qt::Uninitialized);
    auto *first = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, first, &size) != NO_ERROR)
        return Unknown;

    for (auto *a = first; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        if (!a->FirstGatewayAddress)
            continue;   // no default route through here, so not the one in use
        return onOff(a->Flags & IP_ADAPTER_DHCP_ENABLED);
    }
    return Unknown;
}

#endif   // Q_OS_WIN

} // namespace

Probe::Probe(QObject *parent)
    : QObject(parent)
{
}

void Probe::start()
{
    if (m_started)
        return;
    m_started = true;

    // Off the current call stack: start() is called while the window is still being
    // wired up, and the instant stage touches a few dozen registry keys.
    QTimer::singleShot(0, this, [this] {
        runInstant();
        Q_EMIT updated(Stage::Instant);
        runInventory();
    });
}

// ---------------------------------------------------------------------------
// Stage 1 — registry and Win32 only
// ---------------------------------------------------------------------------

void Probe::runInstant()
{
#ifdef Q_OS_WIN
    // --- privacy scorecard --------------------------------------------------
    //
    // Five switches, each read where Windows actually reads it: policy first, because a
    // policy value is what wins, then the per-user setting. "Restricted" is counted so
    // the block can end on one number instead of asking the eye to add up five rows.
    int restricted = 0;
    {
        QSettings policy = hklm(QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection"));
        QSettings local = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection"));
        int level = dword(policy, "AllowTelemetry", -1);
        if (level < 0)
            level = dword(local, "AllowTelemetry", -1);
        switch (level) {
        case 0: m_facts.privacyTelemetry = word("deep.telemetry.security"); ++restricted; break;
        case 1: m_facts.privacyTelemetry = word("deep.telemetry.required"); ++restricted; break;
        case 2: m_facts.privacyTelemetry = word("deep.telemetry.enhanced"); break;
        case 3: m_facts.privacyTelemetry = word("deep.telemetry.optional"); break;
        default: m_facts.privacyTelemetry = word("deep.telemetry.unset"); break;
        }
    }
    {
        QSettings ad = hkcu(QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo"));
        const bool on = dword(ad, "Enabled", 1) != 0;
        m_facts.privacyAdvertisingId = onOff(on);
        if (!on)
            ++restricted;
    }
    {
        QSettings policy = hklm(QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\System"));
        const bool on = dword(policy, "PublishUserActivities", 1) != 0;
        m_facts.privacyActivityHistory = onOff(on);
        if (!on)
            ++restricted;
    }
    {
        QSettings consent = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
                                                "\\CapabilityAccessManager\\ConsentStore\\location"));
        const QString value = consent.value(QStringLiteral("Value")).toString();
        if (value.isEmpty()) {
            m_facts.privacyLocation = Unknown;
        } else {
            const bool allowed = value.compare(QLatin1String("Allow"), Qt::CaseInsensitive) == 0;
            m_facts.privacyLocation = onOff(allowed);
            if (!allowed)
                ++restricted;
        }
    }
    {
        // The switch Settings calls "inking and typing personalisation" is stored the
        // other way round: 1 means collection is *restricted*.
        QSettings ink = hkcu(QStringLiteral("Software\\Microsoft\\InputPersonalization"));
        const bool collecting = dword(ink, "RestrictImplicitTextCollection", 0) == 0;
        m_facts.privacyInkTyping = onOff(collecting);
        if (!collecting)
            ++restricted;
    }
    m_facts.privacyScore = Locale::tr(QStringLiteral("deep.privacyScoreValue")).arg(restricted);

    // --- accounts & UAC -----------------------------------------------------
    {
        QSettings system = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
                                               "\\Policies\\System"));
        const int enableLua = dword(system, "EnableLUA", 1);
        const int consent = dword(system, "ConsentPromptBehaviorAdmin", 5);
        const int secureDesktop = dword(system, "PromptOnSecureDesktop", 1);

        if (enableLua == 0)
            m_facts.uacLevel = word("deep.uac.off");
        else if (consent == 0)
            m_facts.uacLevel = word("deep.uac.never");
        else if (consent == 2 && secureDesktop != 0)
            m_facts.uacLevel = word("deep.uac.always");
        else if (secureDesktop == 0)
            m_facts.uacLevel = word("deep.uac.noDim");
        else
            m_facts.uacLevel = word("deep.uac.default");
    }

    // --- virtualisation & isolation ----------------------------------------
    {
        QSettings guard = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\DeviceGuard"));
        QSettings hvci = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\DeviceGuard"
                                             "\\Scenarios\\HypervisorEnforcedCodeIntegrity"));
        const bool vbsOn = dword(guard, "EnableVirtualizationBasedSecurity", 0) != 0;
        const bool hvciOn = dword(hvci, "Enabled", 0) != 0;
        m_facts.vbs = QStringLiteral("%1 / %2").arg(onOff(vbsOn), onOff(hvciOn));

        QSettings lsa = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\Lsa"));
        m_facts.credentialGuard = onOff(dword(lsa, "LsaCfgFlags", 0) != 0);

        // The Hyper-V hypervisor is a service; whether it is configured to launch is the
        // honest answer here, since whether it is *running* needs a CIM query.
        QSettings vmms = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Services\\vmms"));
        const int start = dword(vmms, "Start", -1);
        m_facts.hyperV = start < 0 ? word("deep.notInstalled")
                        : start == 4 ? word("sys.kapali")
                                     : word("sys.acik");

        const QString sandbox = QDir::fromNativeSeparators(qEnvironmentVariable("SystemRoot"))
                                + QStringLiteral("/System32/WindowsSandbox.exe");
        m_facts.sandbox = QFileInfo::exists(sandbox) ? word("deep.installed") : word("deep.notInstalled");

        // WSL names its distributions under the user's own hive, one subkey each.
        QSettings lxss = hkcu(QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Lxss"));
        const QStringList guids = lxss.childGroups();
        QStringList names;
        for (const QString &guid : guids) {
            const QString name =
                hkcu(QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Lxss\\") + guid)
                    .value(QStringLiteral("DistributionName"))
                    .toString();
            if (!name.isEmpty())
                names << name;
        }
        if (names.isEmpty()) {
            m_facts.wsl = word("deep.notInstalled");
        } else {
            const int version = dword(lxss, "DefaultVersion", 0);
            m_facts.wsl = version > 0
                              ? QStringLiteral("%1 · %2").arg(version).arg(names.join(QStringLiteral(", ")))
                              : names.join(QStringLiteral(", "));
        }
    }

    // --- Windows Update, the half that is registry --------------------------
    {
        QSettings ux = hklm(QStringLiteral("SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings"));
        const QString until = ux.value(QStringLiteral("PauseUpdatesExpiryTime")).toString();
        if (until.isEmpty()) {
            m_facts.updatePaused = word("sys.no");
        } else {
            const QDateTime dt = QDateTime::fromString(until, Qt::ISODate);
            m_facts.updatePaused = dt.isValid() && dt > QDateTime::currentDateTimeUtc()
                                       ? Locale::tr(QStringLiteral("deep.pausedUntil"))
                                             .arg(SysInfo::friendlyDateTime(dt.toLocalTime()))
                                       : word("sys.no");
        }

        QSettings host = hklm(QStringLiteral("SOFTWARE\\Microsoft\\WindowsSelfHost\\Applicability"));
        const QString ring = host.value(QStringLiteral("BranchName")).toString();
        m_facts.updateChannel = ring.isEmpty() ? word("deep.channel.retail") : ring;

        QSettings service = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Services\\wuauserv"));
        switch (dword(service, "Start", -1)) {
        case 2: m_facts.updateService = word("svc.opt.auto"); break;
        case 3: m_facts.updateService = word("svc.opt.manual"); break;
        case 4: m_facts.updateService = word("svc.opt.disabled"); break;
        default: break;
        }
    }

    // --- integrity ----------------------------------------------------------
    {
        m_facts.restartReason = pendingRestartReason();

        const QString root = QDir::fromNativeSeparators(qEnvironmentVariable("SystemRoot"));
        QDir dumps(root + QStringLiteral("/Minidump"));
        const QFileInfoList files = dumps.entryInfoList({QStringLiteral("*.dmp")}, QDir::Files,
                                                        QDir::Time);
        if (files.isEmpty()) {
            m_facts.minidumps = word("sys.none");
            m_facts.lastCrash = word("sys.none");
        } else {
            qint64 total = 0;
            for (const QFileInfo &f : files)
                total += f.size();
            m_facts.minidumps = Locale::tr(QStringLiteral("deep.minidumpCount"))
                                    .arg(files.size())
                                    .arg(QLocale().formattedDataSize(total, 1, QLocale::DataSizeTraditionalFormat));

            const QFileInfo &newest = files.first();
            const quint32 code = bugCheckOf(newest.absoluteFilePath());
            const QString when = SysInfo::friendlyDateTime(newest.lastModified());
            m_facts.lastCrash = code != 0 ? QStringLiteral("%1 · %2").arg(when, bugCheckName(code))
                                          : when;
        }
    }

    // --- connection ---------------------------------------------------------
    {
        m_facts.dhcp = dhcpState();
        m_facts.activeConnections = establishedConnections();

        QSettings internet = hkcu(QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion"
                                                 "\\Internet Settings"));
        if (dword(internet, "ProxyEnable", 0) != 0) {
            const QString server = internet.value(QStringLiteral("ProxyServer")).toString();
            m_facts.proxy = server.isEmpty() ? word("sys.acik") : server;
        } else {
            m_facts.proxy = word("sys.no");
        }

        // 2 is "DoH where the resolver supports it", 3 is "DoH only".
        QSettings dns = hklm(QStringLiteral("SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters"));
        const int doh = dword(dns, "EnableAutoDoh", 0);
        m_facts.doh = doh >= 2 ? word("sys.acik") : word("sys.kapali");

        QSettings cost = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"
                                             "\\NetworkList\\DefaultMediaCost"));
        // 1 unrestricted · 2 fixed · 4 variable — anything above 1 is metered.
        const int ethernet = dword(cost, "Ethernet", 1);
        const int wifi = dword(cost, "WiFi", 1);
        m_facts.metered = (ethernet > 1 || wifi > 1) ? word("sys.yes") : word("sys.no");
    }

    // --- performance baseline, the registry half ----------------------------
    {
        QSettings winsat = hklm(QStringLiteral("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinSAT"));
        const QStringList scores{
            winsat.value(QStringLiteral("CpuScore")).toString(),
            winsat.value(QStringLiteral("MemoryScore")).toString(),
            winsat.value(QStringLiteral("DiskScore")).toString(),
            winsat.value(QStringLiteral("GraphicsScore")).toString(),
        };
        if (!scores.first().isEmpty() && !scores.contains(QString()))
            m_facts.winsat = scores.join(QStringLiteral(" / "));
        else
            m_facts.winsat = word("deep.winsatNotRun");

        PERFORMANCE_INFORMATION perf{};
        perf.cb = sizeof(perf);
        if (GetPerformanceInfo(&perf, sizeof(perf)) && perf.CommitLimit > 0) {
            const quint64 used = quint64(perf.CommitTotal) * perf.PageSize;
            const quint64 limit = quint64(perf.CommitLimit) * perf.PageSize;
            m_facts.commitCharge =
                QStringLiteral("%1 / %2 · %3")
                    .arg(QLocale().formattedDataSize(qint64(used), 1, QLocale::DataSizeTraditionalFormat),
                         QLocale().formattedDataSize(qint64(limit), 1, QLocale::DataSizeTraditionalFormat),
                         Locale::tr(QStringLiteral("sys.percentValue"))
                             .arg(qRound(100.0 * qreal(used) / qreal(limit))));
        }
    }
#endif   // Q_OS_WIN
}

// ---------------------------------------------------------------------------
// The two PowerShell stages
// ---------------------------------------------------------------------------

void Probe::runScript(const char *script, void (Probe::*then)(const QJsonObject &))
{
#ifdef Q_OS_WIN
    // Absolute rather than resolved through PATH, for both reasons in winpaths.h: this
    // is an elevated process, and findExecutable() blocks on an unreachable PATH entry.
    const QString powershell = WinPaths::powershell();
    if (powershell.isEmpty()) {
        (this->*then)({});
        return;
    }

    auto *process = new QProcess(this);
    // Both handlers guard on the pointer they were handed rather than on a member: a
    // crashed process emits errorOccurred and finished both, and whichever arrives first
    // must be the only one that reports.
    const auto settle = [this, process, then](bool ok) {
        if (process->property("settled").toBool())
            return;
        process->setProperty("settled", true);
        QJsonObject parsed;
        if (ok) {
            const QByteArray out = process->readAllStandardOutput().trimmed();
            parsed = QJsonDocument::fromJson(out).object();
        }
        process->deleteLater();
        (this->*then)(parsed);
    };

    connect(process, &QProcess::finished, this,
            [settle](int code, QProcess::ExitStatus) { settle(code == 0); });
    connect(process, &QProcess::errorOccurred, this,
            [settle](QProcess::ProcessError) { settle(false); });

    // Two encoding fixes, and between them they are the difference between these stages
    // working and returning nothing at all on a machine that is not in English.
    //
    // The console first. A GUI process starting powershell.exe gets a fresh console whose
    // output code page is the system OEM one — 857 on a Turkish install — so a single
    // non-ASCII character anywhere in the answer (a disk's FriendlyName, a Wi-Fi SSID, a
    // localised fsutil line, the '·' below) reached Qt as a byte that is not valid UTF-8.
    // Qt's JSON parser does not skip it: it fails the whole document with
    // IllegalUTF8String, fromJson() returns null, and applyInventory/applyHardware take
    // their "nothing came back" branch. One character, and the entire stage is discarded.
    //
    // Then the script itself, which is a UTF-8 literal in a UTF-8 source file: reading it
    // back as Latin-1 turned the '·' both scripts embed into 'Â·' before PowerShell ever
    // saw it.
    const QString preamble =
        QStringLiteral("[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false; "
                       "$OutputEncoding = [Console]::OutputEncoding; ");

    process->start(powershell,
                   {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                    QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                    QStringLiteral("-Command"), preamble + QString::fromUtf8(script)});
#else
    Q_UNUSED(script);
    (this->*then)({});
#endif
}

void Probe::runInventory()
{
    // Counting, mostly: accounts, tasks, drivers, updates, and the two event-log figures
    // that say whether this machine has been having a bad week.
    //
    // Every query is wrapped so one unavailable provider cannot take the rest of the
    // object with it — a machine with Group Policy blocking the Update agent should
    // still get its task and driver counts.
    static const char script[] = R"PS(
$ErrorActionPreference='SilentlyContinue'
$ProgressPreference='SilentlyContinue'

$users = @(Get-LocalUser)
$admins = @()
try { $admins = @(Get-LocalGroupMember -SID 'S-1-5-32-544') } catch {}
$guest = Get-LocalUser -SID 'S-1-5-32-546' 2>$null
if (-not $guest) { $guest = $users | Where-Object { $_.SID.Value -like '*-501' } | Select-Object -First 1 }
$me = $users | Where-Object { $_.Name -eq $env:USERNAME } | Select-Object -First 1

$tasks = @(Get-ScheduledTask)
$telemetryNames = @('Consolidator','UsbCeip','Microsoft Compatibility Appraiser','ProgramDataUpdater','KernelCeipTask','DiskDiagnosticDataCollector','QueueReporting')
$telemetry = @($tasks | Where-Object { $telemetryNames -contains $_.TaskName })
# Windows' own tasks live under \Microsoft\*, so "everything else" is nearly the right
# test for a third-party one — except that the root path is exactly where installers put
# theirs, and excluding it undercounted by about five to one. These are the few tasks
# Microsoft does register at the root, and they are the ones to subtract instead.
$msRoot = @('CreateExplorerShellUnelevatedTask','MicrosoftEdgeUpdateTaskMachineCore','MicrosoftEdgeUpdateTaskMachineUA')

$pnp = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.ConfigManagerErrorCode -ne 0 })
$drivers = @(Get-CimInstance Win32_PnPSignedDriver)
$newest = $drivers | Where-Object { $_.DriverDate } | Sort-Object DriverDate -Descending | Select-Object -First 1

$pending = -1
$lastCheck = ''
try {
  $searcher = (New-Object -ComObject Microsoft.Update.Session).CreateUpdateSearcher()
  $pending = @($searcher.Search('IsInstalled=0 and IsHidden=0').Updates).Count
} catch {}
try {
  $auto = New-Object -ComObject Microsoft.Update.AutoUpdate
  if ($auto.Results.LastSearchSuccessDate) { $lastCheck = $auto.Results.LastSearchSuccessDate.ToString('o') }
} catch {}

$since = (Get-Date).AddHours(-24)
$critical = @(Get-WinEvent -FilterHashtable @{LogName='System';Level=1,2;StartTime=$since} -ErrorAction SilentlyContinue)
$boot = Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-Diagnostics-Performance/Operational';Id=100} -MaxEvents 1 -ErrorAction SilentlyContinue
$bootMs = -1
# Property 5 is BootTime, the milliseconds the boot took. Property 0 is BootTsVersion,
# which is the constant 2 on every machine and used to be what this read — so the
# row said "0.0 s" wherever event 100 existed at all.
if ($boot) { try { $bootMs = [int]$boot.Properties[5].Value } catch {} }

$pf = Get-CimInstance Win32_PageFileUsage | Select-Object -First 1

[pscustomobject]@{
  accountCount   = $users.Count
  adminCount     = $admins.Count
  guestEnabled   = if ($guest) { [bool]$guest.Enabled } else { $false }
  guestPresent   = [bool]$guest
  passwordNeverExpires = if ($me) { [bool]$me.PasswordNeverExpires } else { $false }
  passwordLastSet= if ($me -and $me.PasswordLastSet) { $me.PasswordLastSet.ToString('o') } else { '' }
  taskTotal      = $tasks.Count
  taskDisabled   = @($tasks | Where-Object { $_.State -eq 'Disabled' }).Count
  taskTelemetryOff = @($telemetry | Where-Object { $_.State -eq 'Disabled' }).Count
  taskTelemetryTotal = $telemetry.Count
  taskThirdParty = @($tasks | Where-Object { $_.TaskPath -notlike '\Microsoft\*' -and
    -not ($_.TaskPath -eq '\' -and ($msRoot -contains $_.TaskName -or $_.TaskName -like 'OneDrive*')) }).Count
  driverProblem  = $pnp.Count
  driverProblemName = if ($pnp.Count -gt 0) { [string]$pnp[0].Name + ' · ' + [string]$pnp[0].ConfigManagerErrorCode } else { '' }
  driverTotal    = $drivers.Count
  driverUnsigned = @($drivers | Where-Object { $_.IsSigned -eq $false }).Count
  driverLatest   = if ($newest) { [string]$newest.DeviceName + ' ' + [string]$newest.DriverVersion } else { '' }
  driverLatestDate = if ($newest -and $newest.DriverDate) { $newest.DriverDate.ToString('o') } else { '' }
  updatePending  = $pending
  updateLastCheck= $lastCheck
  criticalEvents = $critical.Count
  bootMs         = $bootMs
  pageFileSize   = if ($pf) { [int]$pf.AllocatedBaseSize } else { -1 }
  pageFileUsed   = if ($pf) { [int]$pf.CurrentUsage } else { -1 }
} | ConvertTo-Json -Compress
)PS";

    runScript(script, &Probe::applyInventory);
}

void Probe::applyInventory(const QJsonObject &o)
{
    if (!m_replaying)
        m_inventory = o;

    if (!o.isEmpty()) {
        // --- accounts -------------------------------------------------------
        const int accounts = o.value(QStringLiteral("accountCount")).toInt(-1);
        const int admins = o.value(QStringLiteral("adminCount")).toInt(-1);
        if (accounts >= 0)
            m_facts.accountsLocal = admins >= 0
                                        ? Locale::tr(QStringLiteral("deep.accountsValue"))
                                              .arg(accounts).arg(admins)
                                        : QString::number(accounts);
        if (o.contains(QStringLiteral("guestPresent")))
            m_facts.accountsGuest = !o.value(QStringLiteral("guestPresent")).toBool()
                                        ? word("sys.none")
                                        : onOff(o.value(QStringLiteral("guestEnabled")).toBool());

        m_facts.passwordPolicy = o.value(QStringLiteral("passwordNeverExpires")).toBool()
                                     ? word("deep.passwordNeverExpires")
                                     : word("deep.passwordExpires");

        const QString set = o.value(QStringLiteral("passwordLastSet")).toString();
        if (!set.isEmpty()) {
            const QDateTime dt = QDateTime::fromString(set, Qt::ISODateWithMs);
            if (dt.isValid()) {
                const qint64 days = dt.daysTo(QDateTime::currentDateTime());
                m_facts.passwordAge = Locale::tr(QStringLiteral("deep.daysAgo")).arg(days);
            }
        }

        // --- scheduled tasks ------------------------------------------------
        m_facts.taskTotal = count(o, "taskTotal");
        m_facts.taskDisabled = count(o, "taskDisabled");
        m_facts.taskThirdParty = count(o, "taskThirdParty");
        const int telemetryTotal = o.value(QStringLiteral("taskTelemetryTotal")).toInt(-1);
        if (telemetryTotal >= 0)
            m_facts.taskTelemetry = Locale::tr(QStringLiteral("deep.telemetryTasks"))
                                        .arg(o.value(QStringLiteral("taskTelemetryOff")).toInt())
                                        .arg(telemetryTotal);

        // --- drivers --------------------------------------------------------
        const int problem = o.value(QStringLiteral("driverProblem")).toInt(-1);
        if (problem > 0) {
            const QString name = o.value(QStringLiteral("driverProblemName")).toString();
            m_facts.driverProblem = name.isEmpty() ? QString::number(problem)
                                                   : QStringLiteral("%1 · %2").arg(problem).arg(name);
        } else if (problem == 0) {
            m_facts.driverProblem = word("sys.none");
        }
        m_facts.driverTotal = count(o, "driverTotal");
        const int unsignedCount = o.value(QStringLiteral("driverUnsigned")).toInt(-1);
        if (unsignedCount >= 0)
            m_facts.driverUnsigned = unsignedCount == 0 ? word("sys.none")
                                                        : QString::number(unsignedCount);

        const QString latest = o.value(QStringLiteral("driverLatest")).toString().simplified();
        if (!latest.isEmpty()) {
            const QDateTime dt = QDateTime::fromString(
                o.value(QStringLiteral("driverLatestDate")).toString(), Qt::ISODateWithMs);
            m_facts.driverLatest = dt.isValid()
                                       ? QStringLiteral("%1 · %2").arg(latest,
                                                                       QLocale().toString(dt.date(), QLocale::ShortFormat))
                                       : latest;
        }

        // --- Windows Update -------------------------------------------------
        const int pending = o.value(QStringLiteral("updatePending")).toInt(-1);
        if (pending >= 0)
            m_facts.updatePending = pending == 0
                                        ? word("deep.upToDate")
                                        : Locale::tr(QStringLiteral("deep.updateCount")).arg(pending);

        const QDateTime checked = QDateTime::fromString(
            o.value(QStringLiteral("updateLastCheck")).toString(), Qt::ISODateWithMs);
        if (checked.isValid())
            m_facts.updateLastCheck = SysInfo::friendlyDateTime(checked.toLocalTime());

        // --- integrity & performance ---------------------------------------
        const int critical = o.value(QStringLiteral("criticalEvents")).toInt(-1);
        if (critical >= 0)
            m_facts.criticalEvents = Locale::tr(QStringLiteral("deep.criticalEventsValue")).arg(critical);

        const int bootMs = o.value(QStringLiteral("bootMs")).toInt(-1);
        if (bootMs > 0)
            m_facts.bootDuration = Locale::tr(QStringLiteral("deep.seconds"))
                                       .arg(QLocale().toString(bootMs / 1000.0, 'f', 1));

        const int pfSize = o.value(QStringLiteral("pageFileSize")).toInt(-1);
        const int pfUsed = o.value(QStringLiteral("pageFileUsed")).toInt(-1);
        if (pfSize > 0)
            m_facts.pageFileUsage = pfUsed >= 0
                                        ? QStringLiteral("%1 / %2 MB").arg(pfUsed).arg(pfSize)
                                        : QStringLiteral("%1 MB").arg(pfSize);
        else if (pfSize == 0)
            m_facts.pageFileUsage = word("sys.none");
    }

    // A replay is rebuilding text that is already on screen, so it neither advances to
    // the next stage nor asks the page to redraw between them; retranslate() does that
    // once, at the end.
    if (!m_replaying) {
        Q_EMIT updated(Stage::Inventory);
        runHardware();
    }
}

void Probe::runHardware()
{
    // The parts that ask the hardware rather than Windows: encryption state per volume,
    // SMART reliability counters, the battery's own capacity registers, thermal zones.
    // Several of these are unavailable on a desktop, on a VM, or without an elevated
    // token, so each is wrapped and each blank comes back as an empty string.
    static const char script[] = R"PS(
$ErrorActionPreference='SilentlyContinue'
$ProgressPreference='SilentlyContinue'

$volumes = @()
try {
  foreach ($v in @(Get-BitLockerVolume)) {
    $volumes += [pscustomobject]@{
      name = [string]$v.MountPoint
      status = [string]$v.VolumeStatus
      method = [string]$v.EncryptionMethod
      protection = [string]$v.ProtectionStatus
      percent = [int]$v.EncryptionPercentage
      keyProtector = ($v.KeyProtector | ForEach-Object { [string]$_.KeyProtectorType }) -join ','
    }
  }
} catch {}

$disks = @()
try {
  foreach ($d in @(Get-PhysicalDisk)) {
    $rc = $null
    try { $rc = $d | Get-StorageReliabilityCounter } catch {}
    $disks += [pscustomobject]@{
      name = [string]$d.FriendlyName
      health = [string]$d.HealthStatus
      media = [string]$d.MediaType
      sizeGB = [int]($d.Size / 1GB)
      wear = if ($rc -and $rc.Wear -ne $null) { [int]$rc.Wear } else { -1 }
      hours = if ($rc -and $rc.PowerOnHours -ne $null) { [int]$rc.PowerOnHours } else { -1 }
      writtenGB = if ($rc -and $rc.TotalBytesWritten) { [int64]($rc.TotalBytesWritten / 1GB) } else { -1 }
      temperature = if ($rc -and $rc.Temperature) { [int]$rc.Temperature } else { -1 }
    }
  }
} catch {}

$style = ''
try { $style = (@(Get-Disk | Where-Object { $_.IsBoot }) | Select-Object -First 1).PartitionStyle } catch {}
$trim = ''
try { $trim = (fsutil behavior query DisableDeleteNotify) -join ' ' } catch {}

$temp = -1
try {
  $tz = Get-CimInstance -Namespace root/WMI -ClassName MSAcpi_ThermalZoneTemperature | Select-Object -First 1
  if ($tz) { $temp = [int](($tz.CurrentTemperature / 10) - 273.15) }
} catch {}
$fan = -1
try { $f = Get-CimInstance Win32_Fan | Select-Object -First 1; if ($f -and $f.DesiredSpeed) { $fan = [int]$f.DesiredSpeed } } catch {}

$designCapacity = -1; $fullCapacity = -1; $cycles = -1
try {
  $static = Get-CimInstance -Namespace root/WMI -ClassName BatteryStaticData | Select-Object -First 1
  if ($static) { $designCapacity = [int]$static.DesignedCapacity; $cycles = [int]$static.CycleCount }
  $full = Get-CimInstance -Namespace root/WMI -ClassName BatteryFullChargedCapacity | Select-Object -First 1
  if ($full) { $fullCapacity = [int]$full.FullChargedCapacity }
} catch {}

$tpmOwned = ''
try {
  $tpm = Get-CimInstance -Namespace root/CIMV2/Security/MicrosoftTpm -ClassName Win32_Tpm |
         Select-Object -First 1
  if ($tpm) { $tpmOwned = if ($tpm.IsOwned_InitialValue) { 'yes' } else { 'no' } }
} catch {}

$wifi = ''
try {
  $lines = netsh wlan show interfaces 2>$null
  # The SSID label is "SSID" in every language; the "Signal" label is not — it comes out
  # of wlancfg.dll.mui in the display language — so the signal is picked out by its shape,
  # the one line that ends in a percentage. -First 1 because a second WLAN adapter turns
  # .Matches into an array and Groups[1] then indexes the wrong thing.
  $ssid = ($lines | Select-String -Pattern '^\s+SSID\s+:\s+(.+)$' | Select-Object -First 1).Matches.Groups[1].Value
  $signal = ($lines | Select-String -Pattern ':\s+(\d{1,3}%)\s*$' | Select-Object -First 1).Matches.Groups[1].Value
  if ($ssid) { $wifi = if ($signal) { "$ssid · $signal" } else { $ssid } }
} catch {}

[pscustomobject]@{
  volumes = $volumes
  disks = $disks
  partitionStyle = [string]$style
  trim = [string]$trim
  temperature = $temp
  fan = $fan
  batteryDesign = $designCapacity
  batteryFull = $fullCapacity
  batteryCycles = $cycles
  tpmOwned = [string]$tpmOwned
  wifi = [string]$wifi
} | ConvertTo-Json -Compress -Depth 4
)PS";

    runScript(script, &Probe::applyHardware);
}

void Probe::applyHardware(const QJsonObject &o)
{
    if (!m_replaying)
        m_hardware = o;

    if (!o.isEmpty()) {
        // --- encryption -----------------------------------------------------
        const QJsonArray volumes = o.value(QStringLiteral("volumes")).toArray();
        QStringList protectors;
        for (const QJsonValue &v : volumes) {
            const QJsonObject vo = v.toObject();
            const QString status = vo.value(QStringLiteral("status")).toString();
            const bool encrypted = status.compare(QLatin1String("FullyEncrypted"),
                                                  Qt::CaseInsensitive) == 0;
            const bool partial = status.compare(QLatin1String("EncryptionInProgress"),
                                                Qt::CaseInsensitive) == 0;

            QString detail;
            if (encrypted) {
                detail = QStringLiteral("BitLocker · %1")
                             .arg(vo.value(QStringLiteral("method")).toString());
            } else if (partial) {
                detail = Locale::tr(QStringLiteral("deep.encrypting"))
                             .arg(vo.value(QStringLiteral("percent")).toInt());
            } else {
                detail = word("deep.unencrypted");
            }

            Entry entry;
            entry.name = vo.value(QStringLiteral("name")).toString();
            entry.detail = detail;
            entry.meter = partial ? vo.value(QStringLiteral("percent")).toInt() / 100.0 : -1.0;
            m_facts.encryption.append(entry);

            protectors << vo.value(QStringLiteral("keyProtector")).toString();
        }
        // Where the recovery key can actually be retrieved from, which is the part of
        // "your disk is encrypted" that decides whether the data is recoverable.
        const QString joined = protectors.join(QLatin1Char(','));
        if (joined.contains(QLatin1String("RecoveryPassword")))
            m_facts.recoveryKey = word("deep.recoveryStored");
        else if (!volumes.isEmpty())
            m_facts.recoveryKey = word("deep.recoveryNone");

        // --- disk health ----------------------------------------------------
        const QJsonArray disks = o.value(QStringLiteral("disks")).toArray();
        for (const QJsonValue &d : disks) {
            const QJsonObject dobj = d.toObject();
            QStringList parts;

            const QString health = dobj.value(QStringLiteral("health")).toString();
            if (health.compare(QLatin1String("Healthy"), Qt::CaseInsensitive) == 0)
                parts << word("deep.healthy");
            else if (!health.isEmpty())
                parts << health;

            const int wear = dobj.value(QStringLiteral("wear")).toInt(-1);
            if (wear >= 0)
                parts << Locale::tr(QStringLiteral("deep.lifeLeft")).arg(100 - wear);

            const int hours = dobj.value(QStringLiteral("hours")).toInt(-1);
            if (hours >= 0)
                parts << Locale::tr(QStringLiteral("deep.powerOnHours"))
                             .arg(QLocale().toString(hours));

            const qint64 written = qint64(dobj.value(QStringLiteral("writtenGB")).toDouble(-1));
            if (written >= 0)
                parts << Locale::tr(QStringLiteral("deep.written"))
                             .arg(written >= 1024
                                      ? QStringLiteral("%1 TB").arg(QLocale().toString(written / 1024.0, 'f', 1))
                                      : QStringLiteral("%1 GB").arg(written));

            const int temperature = dobj.value(QStringLiteral("temperature")).toInt(-1);
            if (temperature > 0)
                parts << QStringLiteral("%1 °C").arg(temperature);

            Entry entry;
            entry.name = dobj.value(QStringLiteral("name")).toString();
            entry.detail = parts.isEmpty() ? Unknown : parts.join(QStringLiteral(" · "));
            entry.meter = wear >= 0 ? (100 - wear) / 100.0 : -1.0;
            m_facts.disks.append(entry);
        }

        const QString style = o.value(QStringLiteral("partitionStyle")).toString();
        if (!style.isEmpty())
            m_facts.partitionStyle = style;

        // fsutil prints "DisableDeleteNotify = 0" when TRIM is on; the text around it is
        // localised by Windows, the number is not.
        const QString trim = o.value(QStringLiteral("trim")).toString();
        if (trim.contains(QLatin1String("= 0")) || trim.contains(QLatin1String("=0")))
            m_facts.trim = word("sys.acik");
        else if (trim.contains(QLatin1String("= 1")) || trim.contains(QLatin1String("=1")))
            m_facts.trim = word("sys.kapali");

        // --- sensors --------------------------------------------------------
        const int temperature = o.value(QStringLiteral("temperature")).toInt(-1);
        // A thermal zone that reports absolute zero is a zone that is not wired up.
        if (temperature > 0 && temperature < 150)
            m_facts.cpuTemperature = QStringLiteral("%1 °C").arg(temperature);
        else
            m_facts.cpuTemperature = word("deep.notReadable");

        const int fan = o.value(QStringLiteral("fan")).toInt(-1);
        m_facts.fan = fan > 0 ? Locale::tr(QStringLiteral("deep.rpm")).arg(fan)
                              : word("deep.notReadable");

        const int design = o.value(QStringLiteral("batteryDesign")).toInt(-1);
        const int full = o.value(QStringLiteral("batteryFull")).toInt(-1);
        const int cycleCount = o.value(QStringLiteral("batteryCycles")).toInt(-1);
        if (design > 0 && full > 0) {
            // Through the table, because the percent sign goes before the number in
            // Turkish and after it almost everywhere else — the literal here was the
            // Turkish shape, so nine languages read "%92".
            m_facts.batteryHealth = Locale::tr(QStringLiteral("deep.batteryCapacity"))
                                        .arg(QString::number(qRound(100.0 * full / design)),
                                             QLocale().toString(full),
                                             QLocale().toString(design));
        } else if (design > 0 || cycleCount > 0) {
            // BatteryStaticData answered something, so there is a battery; only the
            // capacity registers are missing.
            m_facts.batteryHealth = word("deep.notReadable");
        } else {
            // Nothing came back at all, which is two different situations: a desktop, and
            // a laptop whose firmware does not implement the WMI battery classes. Windows
            // itself knows which, and saying "yok" about a battery that is sitting right
            // there is the kind of confident wrong answer this page is meant to avoid.
            m_facts.batteryHealth = hasBattery() ? word("deep.notReadable") : word("sys.none");
        }

        const int cycles = o.value(QStringLiteral("batteryCycles")).toInt(-1);
        if (cycles > 0)
            m_facts.batteryCycles = QLocale().toString(cycles);

        // Whether the TPM has an owner, which is the half of "you have a TPM" that decides
        // whether BitLocker can seal a key to it. The Encryption block has rendered this
        // row since 0.9.8; nothing ever filled it, so it read blank on every machine.
        // A VM or a firmware with no TPM answers nothing at all and the row keeps its dash.
        const QString tpmOwned = o.value(QStringLiteral("tpmOwned")).toString();
        if (tpmOwned == QLatin1String("yes"))
            m_facts.tpmOwnership = word("deep.tpmOwned");
        else if (tpmOwned == QLatin1String("no"))
            m_facts.tpmOwnership = word("deep.tpmUnowned");

        const QString wifi = o.value(QStringLiteral("wifi")).toString().trimmed();
        m_facts.wifi = wifi.isEmpty() ? word("deep.noWifi") : wifi;
    }

    if (!m_replaying)
        Q_EMIT updated(Stage::Hardware);
}

void Probe::retranslate()
{
    // Before the first run there is nothing to rebuild, and start() will read everything
    // in whatever language is current by then.
    if (!m_started)
        return;

    m_replaying = true;

    // Back to a struct of dashes first. The two list-shaped blocks — encrypted volumes
    // and physical disks — are appended to rather than assigned, so replaying on top of
    // the old Facts would show every drive twice.
    m_facts = Facts{};

    runInstant();
    if (!m_inventory.isEmpty())
        applyInventory(m_inventory);
    if (!m_hardware.isEmpty())
        applyHardware(m_hardware);

    m_replaying = false;
    Q_EMIT updated(Stage::Hardware);
}

} // namespace DeepInfo
