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

#include <QObject>
#include <QString>
#include <QVector>

namespace DeepInfo {

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
    QString updatePending;
    QString updateLastCheck;
    QString updatePaused;
    QString updateChannel;
    QString updateService;

    // --- system integrity & crash history ----------------------------------
    QString lastCrash;
    QString criticalEvents;
    QString restartReason;
    QString minidumps;

    // --- scheduled tasks ---------------------------------------------------
    QString taskTotal;
    QString taskDisabled;
    QString taskTelemetry;
    QString taskThirdParty;

    // --- drivers -----------------------------------------------------------
    QString driverProblem;
    QString driverUnsigned;
    QString driverTotal;
    QString driverLatest;

    // --- privacy scorecard -------------------------------------------------
    QString privacyTelemetry;
    QString privacyAdvertisingId;
    QString privacyActivityHistory;
    QString privacyLocation;
    QString privacyInkTyping;
    QString privacyScore;

    // --- encryption --------------------------------------------------------
    QVector<Entry> encryption;
    QString recoveryKey;
    QString tpmOwnership;

    // --- accounts & UAC ----------------------------------------------------
    QString accountsLocal;
    QString accountsGuest;
    QString uacLevel;
    QString passwordPolicy;
    QString passwordAge;

    // --- virtualisation & isolation ----------------------------------------
    QString hyperV;
    QString vbs;
    QString wsl;
    QString sandbox;
    QString credentialGuard;

    // --- disk health -------------------------------------------------------
    QVector<Entry> disks;
    QString trim;
    QString partitionStyle;

    // --- performance baseline ----------------------------------------------
    QString winsat;
    QString bootDuration;
    QString pageFileUsage;
    QString commitCharge;

    // --- connection detail -------------------------------------------------
    QString dhcp;
    QString proxy;
    QString doh;
    QString wifi;
    QString activeConnections;
    QString metered;

    // --- sensors, fan & battery health -------------------------------------
    QString cpuTemperature;
    QString batteryHealth;
    QString batteryCycles;
    QString fan;
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

Q_SIGNALS:
    /// \a stage has landed and facts() carries its answers.
    void updated(Stage stage);

private:
    void runInstant();
    void runInventory();
    void runHardware();

    /// Runs \a script through PowerShell and hands the parsed JSON object to \a then.
    /// A run that fails calls \a then with an empty object, so the caller has one path.
    void runScript(const char *script, void (Probe::*then)(const class QJsonObject &));

    void applyInventory(const class QJsonObject &o);
    void applyHardware(const class QJsonObject &o);

    Facts m_facts;
    bool m_started = false;
};

} // namespace DeepInfo
