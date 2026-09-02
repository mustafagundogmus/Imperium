// deepinfo.h — the facts the Genel Bakış page cannot read in a single frame.
//
// SysInfo::collect() is deliberately cheap: registry reads and plain Win32 calls, fast
// enough to run before the window is shown. Everything here is the other kind — a CIM
// query, an event-log scan, a SMART counter, an enumeration of every scheduled task on
// the machine. None of it belongs on the startup path, so none of it is on it.
//
// Three stages, each publishing what it has as soon as it has it:
//
//   Stage::Instant     registry and Win32 only, no child process. Runs synchronously on
//                      the first tick after the window is up, so the blocks it feeds are
//                      populated before the eye has finished travelling down the page.
//   Stage::Inventory   one PowerShell run: accounts, scheduled tasks, drivers, Windows
//                      Update, the event log. Counting things, mostly.
//   Stage::Hardware    a second run for the parts that ask the hardware itself —
//                      BitLocker, SMART, battery chemistry, thermal zones. Slowest, and
//                      last, because it is the least likely to be what you came for.
//
// Every field starts at "—" and only ever moves to a real answer, so a stage that fails
// or a machine that cannot answer leaves the row honest rather than blank. Nothing here
// writes anything, and nothing here needs an elevated token — a few values simply come
// back unavailable without one, and say so.

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace DeepInfo {

/// What a row says before anything has answered it, and what it goes back to saying when
/// the answer is "this machine cannot tell you".
///
/// It lives here rather than in the .cpp because every field of Facts below starts at it.
/// They used to start at an empty QString while the comment above promised otherwise, so
/// a stage that failed — which, before the encoding fix in runScript(), was every stage on
/// a non-English machine — painted a column of blank rows instead of a column of dashes.
inline const QString Unknown = QStringLiteral("—");

/// One row of a list-shaped block (an encrypted volume, a physical disk).
struct Entry
{
    QString name;
    QString detail;
    qreal meter = -1.0;   ///< 0…1 draws a bar on the row; negative hides it
};

struct Facts
{
    // --- Windows Update ----------------------------------------------------
    QString updatePending = Unknown;
    QString updateLastCheck = Unknown;
    QString updatePaused = Unknown;
    QString updateChannel = Unknown;
    QString updateService = Unknown;

    // --- system integrity & crash history ----------------------------------
    QString lastCrash = Unknown;
    QString criticalEvents = Unknown;
    QString restartReason = Unknown;
    QString minidumps = Unknown;

    // --- scheduled tasks ---------------------------------------------------
    QString taskTotal = Unknown;
    QString taskDisabled = Unknown;
    QString taskTelemetry = Unknown;
    QString taskThirdParty = Unknown;

    // --- drivers -----------------------------------------------------------
    QString driverProblem = Unknown;
    QString driverUnsigned = Unknown;
    QString driverTotal = Unknown;
    QString driverLatest = Unknown;

    // --- privacy scorecard -------------------------------------------------
    QString privacyTelemetry = Unknown;
    QString privacyAdvertisingId = Unknown;
    QString privacyActivityHistory = Unknown;
    QString privacyLocation = Unknown;
    QString privacyInkTyping = Unknown;
    QString privacyScore = Unknown;

    // --- encryption --------------------------------------------------------
    QVector<Entry> encryption;
    QString recoveryKey = Unknown;
    QString tpmOwnership = Unknown;

    // --- accounts & UAC ----------------------------------------------------
    QString accountsLocal = Unknown;
    QString accountsGuest = Unknown;
    QString uacLevel = Unknown;
    QString passwordPolicy = Unknown;
    QString passwordAge = Unknown;

    // --- virtualisation & isolation ----------------------------------------
    QString hyperV = Unknown;
    QString vbs = Unknown;
    QString wsl = Unknown;
    QString sandbox = Unknown;
    QString credentialGuard = Unknown;

    // --- disk health -------------------------------------------------------
    QVector<Entry> disks;
    QString trim = Unknown;
    QString partitionStyle = Unknown;

    // --- performance baseline ----------------------------------------------
    QString winsat = Unknown;
    QString bootDuration = Unknown;
    QString pageFileUsage = Unknown;
    QString commitCharge = Unknown;

    // --- connection detail -------------------------------------------------
    QString dhcp = Unknown;
    QString proxy = Unknown;
    QString doh = Unknown;
    QString wifi = Unknown;
    QString activeConnections = Unknown;
    QString metered = Unknown;

    // --- sensors, fan & battery health -------------------------------------
    QString cpuTemperature = Unknown;
    QString gpuTemperature = Unknown;
    QString gpuFan = Unknown;
    QString fan = Unknown;
    QString batteryHealth = Unknown;
    QString batteryCycles = Unknown;
    QString batteryChemistry = Unknown;

    // --- graphics, live ----------------------------------------------------
    QString gpuUtilisation = Unknown;
    QString gpuMemory = Unknown;
    QString gpuPower = Unknown;
    QString gpuClock = Unknown;
    QString gpuPcie = Unknown;

    // --- protection --------------------------------------------------------
    QString antivirus = Unknown;
    QString signatures = Unknown;
    QString lastScan = Unknown;
    QString tamperProtection = Unknown;
    QString firewallProfiles = Unknown;
};

/// Fills a Facts in the background, publishing after each stage.
class Probe : public QObject
{
    Q_OBJECT

public:
    enum class Stage
    {
        Instant,     ///< registry and Win32; no child process
        Inventory,   ///< accounts, tasks, drivers, updates, event log
        Hardware,    ///< BitLocker, SMART, battery, thermal
    };
    Q_ENUM(Stage)

    explicit Probe(QObject *parent = nullptr);

    /// Begins the run. Safe to call more than once; only the first does anything.
    void start();

    /// Everything resolved so far. Fields not yet reached still read "—".
    const Facts &facts() const { return m_facts; }

    /// Rebuilds every answer in the interface's current language, then emits updated().
    ///
    /// Facts holds finished sentences rather than raw readings — "Açık", "3 devre dışı", a
    /// date written the way the locale writes dates — so there is no way to move it to
    /// another language except to build it again. The registry stage is cheap enough to
    /// redo outright; the two PowerShell stages are not, and what they answer does not
    /// depend on the interface language, so they are replayed from the JSON they already
    /// returned. Does nothing before start(), which would read in the new language anyway.
    void retranslate();

Q_SIGNALS:
    /// \a stage has landed and facts() carries its answers.
    void updated(Stage stage);

private:
    void runInstant();
    void runInventory();
    void runHardware();

    /// Runs \a script through PowerShell and hands the parsed JSON object to \a then.
    /// A run that fails calls \a then with an empty object, so the caller has one path.
    void runScript(const char *script, void (Probe::*then)(const QJsonObject &));

    void applyInventory(const QJsonObject &o);
    void applyHardware(const QJsonObject &o);

    Facts m_facts;
    bool m_started = false;

    /// What each script stage answered, kept so retranslate() can rebuild from it
    /// without paying for the run a second time.
    QJsonObject m_inventory;
    QJsonObject m_hardware;

    /// True while retranslate() is replaying: the stages must not chain into the next
    /// one, and the page only wants one refresh at the end rather than three.
    bool m_replaying = false;
};

} // namespace DeepInfo
