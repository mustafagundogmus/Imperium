// features.h — the Özellikler page's rows and runner: WinUtil's Features section.
//
// WinUtil's Config tab starts with nine checkboxes — .NET Framework, Hyper-V, the legacy
// media components, WSL, NFS, the daily registry backup, the two F8 boot-menu rows and
// Windows Sandbox — and an "Install Features" button that hands the ticked ones to
// Invoke-WinUtilFeatureInstall: Enable-WindowsOptionalFeature for every DISM feature the
// row names, then whatever InvokeScript lines it carries. The data is config/feature.json,
// embedded verbatim as :/data/features.json (MIT, resources/licenses/winutil-MIT.txt); only
// its "Features" rows are read here, the Fixes buttons and the legacy panels in the same
// file are not this page's business (God Mode already opens the panels).
//
// What WinUtil does not do, and this page does, is read the machine first. A Windows
// optional feature has a state DISM will report, the boot-menu policy is one bcdedit line,
// and the registry backup is a value and a scheduled task — so every row says whether it
// is already on before offering to turn it on, the way every other row in this app does.
// Disabling a DISM feature is offered on the row as well, which is the one thing here that
// has no WinUtil counterpart; it is Disable-WindowsOptionalFeature and nothing more.

#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QProcess;

namespace Features {

/// One "Features" row of feature.json.
struct Entry
{
    QString key;             ///< WinUtil's checkbox name, e.g. "WPFFeaturewsl"
    QString slug;            ///< "wsl" — the i18n keys are features.item.<slug>.name/.desc
    QString name;            ///< WinUtil's Content, English
    QString description;     ///< WinUtil's Description, English
    QString link;
    QStringList features;    ///< DISM feature names, in order
    QStringList scripts;     ///< InvokeScript entries, in order

    bool isDism() const { return !features.isEmpty(); }
    QString nameKey() const { return QStringLiteral("features.item.") + slug + QStringLiteral(".name"); }
    QString descKey() const { return QStringLiteral("features.item.") + slug + QStringLiteral(".desc"); }
};

/// "WPFFeatureslegacymedia" → "legacymedia", "WPFFeatureRegBackup" → "regbackup".
QString slugFor(const QString &key);

class Catalogue
{
public:
    static const Catalogue &instance();

    /// The "Features" rows, in the file's order, the Install button left out.
    const QVector<Entry> &entries() const { return m_entries; }
    const Entry *entry(const QString &key) const;
    int count() const { return int(m_entries.size()); }

private:
    Catalogue();
    QVector<Entry> m_entries;
    QHash<QString, int> m_index;
};

enum class State { Unknown, Enabled, Disabled, Partial, Unavailable };

/// What the scan found: every optional feature DISM lists with its state, the boot-menu
/// policy, and whether the daily registry backup is wired up.
struct Machine
{
    bool valid = false;
    QHash<QString, QString> featureStates;   ///< FeatureName → "Enabled" / "Disabled" / …
    QString bootMenuPolicy;                  ///< "Legacy", "Standard", or empty when unread
    bool regBackup = false;

    /// A row's position on this machine. A DISM row is Enabled when every feature it
    /// names is, Partial when some are, Unavailable when DISM lists none of them (the
    /// edition does not carry it — Hyper-V and Sandbox on Home). The two F8 rows read the
    /// policy; the backup row reads the value and the task.
    State stateOf(const Entry &e) const;
};

/// The exact lines the install will run, for the confirmation dialog: WinUtil's
/// Enable-WindowsOptionalFeature call per feature, then the row's scripts.
QString installSummary(const QVector<Entry> &entries);
QString disableSummary(const QStringList &features);

class Runner : public QObject
{
    Q_OBJECT

public:
    enum class Job { None, Scan, Install, Disable };
    Q_ENUM(Job)

    explicit Runner(QObject *parent = nullptr);

    bool running() const { return m_process != nullptr; }
    Job job() const { return m_job; }

    /// Get-WindowsOptionalFeature -Online, bcdedit and the backup value, as one JSON.
    void scan();

    /// Invoke-WPFFeatureInstall: Invoke-WinUtilFeatureInstall per row, in order. A row
    /// that throws is reported as failed and the next one still runs.
    void install(const QVector<Entry> &entries);

    /// Disable-WindowsOptionalFeature -Online -NoRestart for each name.
    void disable(const QStringList &features);

Q_SIGNALS:
    void started(Features::Runner::Job job, int total);
    void progress(int done, int total, const QString &key, bool finished, bool ok);
    void line(const QString &text);
    void finished(Features::Runner::Job job, bool ok, int failures);
    void scanned(const Features::Machine &machine);

private:
    void start(Job job, const QString &body, int total);
    void consume();
    void handleLine(const QString &text);
    void onFinished(int exitCode);

    QProcess *m_process = nullptr;
    Job m_job = Job::None;
    int m_total = 0;
    int m_done = 0;
    int m_failures = 0;
    bool m_collecting = false;
    QString m_carry;
    QString m_json;
    QString m_transcript;
    QString m_scriptPath;
    QString m_logPath;
};

} // namespace Features
