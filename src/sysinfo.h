// sysinfo.h — read-only facts about the machine, for the Genel Bakış screen.
//
// Everything here is gathered with plain Win32 calls and registry reads. Nothing is
// written, no elevation is required, and a value that cannot be determined comes back
// as "—" rather than as a guess.
//
// A handful of facts (activation state, last restore point, last successful update)
// are only reachable through CIM, so they are fetched asynchronously by SystemProbe
// and delivered later via updated().

#pragma once

#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>

namespace SysInfo {

struct Facts
{
    // Sistem
    QString osName          = QStringLiteral("—");   ///< "Windows 11 Pro"
    QString version         = QStringLiteral("—");   ///< "24H2 · 26100.4202"
    QString installDate     = QStringLiteral("—");   ///< "12.03.2025"
    QString lastUpdate      = QStringLiteral("—");   ///< "12.08.2026"
    QString activation      = QStringLiteral("—");   ///< "Dijital lisans"
    QString secureBoot      = QStringLiteral("—");   ///< "Açık" / "Kapalı"
    QString tpm             = QStringLiteral("—");   ///< "2.0" / "Yok"

    // Kullanıcı
    QString userName        = QStringLiteral("—");
    QString accountType     = QStringLiteral("—");   ///< "Yerel · Yönetici"
    QString computerName    = QStringLiteral("—");
    QString microsoftAccount= QStringLiteral("—");   ///< "Bağlı" / "Bağlı değil"
    QString activePolicies  = QStringLiteral("—");

    // Donanım
    QString cpu             = QStringLiteral("—");   ///< "Ryzen 7 7800X3D · 8C/16T"
    QString memory          = QStringLiteral("—");   ///< "32 GB DDR5-6000"
    QString gpu             = QStringLiteral("—");   ///< "RTX 4070 · 12 GB"
    QString storage         = QStringLiteral("—");   ///< "NVMe 2 TB · 1.24 TB boş"
    QString motherboard     = QStringLiteral("—");
    QString bios            = QStringLiteral("—");   ///< "1.82 · UEFI"

    // Oturum
    QString uptime          = QStringLiteral("—");   ///< "6 sa 18 dk"
    QString lastBoot        = QStringLiteral("—");   ///< "Bugün 08:03"
    QString lastLogon       = QStringLiteral("—");
    QString pendingRestart  = QStringLiteral("—");   ///< "Yok" / "Var"
    QString lastRestorePoint= QStringLiteral("—");   ///< "Bugün, 14:32"

    // Ekran
    QString displayCount    = QStringLiteral("—");
    QString resolution      = QStringLiteral("—");   ///< "2560×1440 @ 165 Hz"
    QString colorDepth      = QStringLiteral("—");
    QString dpiScale        = QStringLiteral("—");   ///< "%150"
    QString graphicsDriver  = QStringLiteral("—");

    // Ağ
    QString adapter         = QStringLiteral("—");
    QString ipv4            = QStringLiteral("—");
    QString linkSpeed       = QStringLiteral("—");
    QString dnsServer       = QStringLiteral("—");
    QString domain          = QStringLiteral("—");

    // Güç
    QString powerSource     = QStringLiteral("—");   ///< "Şebeke" / "Pil"
    QString battery         = QStringLiteral("—");
    QString powerPlan       = QStringLiteral("—");

    // Güvenlik
    QString defender        = QStringLiteral("—");
    QString firewall        = QStringLiteral("—");
    QString smartScreen     = QStringLiteral("—");
    QString coreIsolation   = QStringLiteral("—");
    QString virtualization  = QStringLiteral("—");

    // Depolama — one row per fixed volume, "C:" → "953 GB · 378 GB boş"
    QVector<QPair<QString, QString>> volumes;

    // Bellenim
    QString manufacturer    = QStringLiteral("—");
    QString model           = QStringLiteral("—");
    QString biosVendor      = QStringLiteral("—");
    QString biosDate        = QStringLiteral("—");
    QString smbios          = QStringLiteral("—");
    QString bootMode        = QStringLiteral("—");   ///< "UEFI" / "Eski (BIOS)"

    // İşlemci ayrıntısı
    QString cpuVendor       = QStringLiteral("—");
    QString cpuBaseClock    = QStringLiteral("—");
    QString cpuArchitecture = QStringLiteral("—");
    QString cpuVirtualization= QStringLiteral("—");

    // Bellek ayrıntısı
    QString memoryInUse     = QStringLiteral("—");
    QString memoryFree      = QStringLiteral("—");
    QString pageFile        = QStringLiteral("—");
    QString memorySlots     = QStringLiteral("—");

    // Yazılım
    QString installedPrograms = QStringLiteral("—");
    QString startupEntries  = QStringLiteral("—");
    QString dotNet          = QStringLiteral("—");
    QString powerShell      = QStringLiteral("—");
    QString defaultBrowser  = QStringLiteral("—");

    // Zaman & bölge
    QString timeZone        = QStringLiteral("—");
    QString ntpServer       = QStringLiteral("—");
    QString locale          = QStringLiteral("—");
    QString keyboardLayout  = QStringLiteral("—");

    // Süreçler
    QString processCount    = QStringLiteral("—");
    QString threadCount     = QStringLiteral("—");
    QString handleCount     = QStringLiteral("—");
    QString idleTime        = QStringLiteral("—");

    // Ek alanlar
    QString buildBranch     = QStringLiteral("—");
    QString editionId       = QStringLiteral("—");
    QString virtualDesktop  = QStringLiteral("—");
    QString ipv6            = QStringLiteral("—");
    QString gateway         = QStringLiteral("—");
    QString adapterCount    = QStringLiteral("—");
    QString profilePath     = QStringLiteral("—");

    // Title bar / header
    QString titleBarSummary = QStringLiteral("—");   ///< "Windows 11 Pro · 26100.4202 · Yönetici"
    bool elevated = false;
};

/// Collects everything that is available synchronously. Cheap enough for startup.
Facts collect();

/// Formats a timestamp the way the design does: "Bugün 08:03", "Dün 22:10",
/// otherwise "12.03.2025 08:03". Pass \a withComma for the "Bugün, 14:32" variant.
QString friendlyDateTime(const QDateTime &dt, bool withComma = false);

/// "6 sa 18 dk" — recomputed on demand because it ticks while the app is open.
QString uptimeString();

/// Live process / thread / handle counts and idle time, refreshed on every sample.
struct LiveCounters
{
    QString processes = QStringLiteral("—");
    QString threads   = QStringLiteral("—");
    QString handles   = QStringLiteral("—");
    QString idle      = QStringLiteral("—");
};
LiveCounters liveCounters();

/// Fills in the CIM-only facts in the background.
class Probe : public QObject
{
    Q_OBJECT

public:
    explicit Probe(QObject *parent = nullptr);

    void start();

Q_SIGNALS:
    /// \a activation, \a restorePoint and \a lastUpdate are empty when unavailable.
    void resolved(const QString &activation, const QString &restorePoint, const QString &lastUpdate);

private:
    bool m_started = false;
};

} // namespace SysInfo
