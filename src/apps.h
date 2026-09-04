// apps.h — the Uygulama kur page's catalogue and its package-manager runner.
//
// This is WinUtil's Install tab, carried over as it is. The catalogue is WinUtil's own
// config/applications.json, embedded verbatim (:/data/applications.json, MIT — see
// resources/licenses/winutil-MIT.txt): 233 programs, each with its WinGet id, its
// Chocolatey id, a category, a one-line description and a link. The runner is WinUtil's
// PowerShell — Test-WinUtilPackageManager, Install-WinUtilWinget, Install-WinUtilChoco,
// Install-WinUtilProgramWinget and Install-WinUtilProgramChoco, written into the script
// the way they appear in functions/private/ — driven by the same three flows the tab
// offers: install or upgrade the selection, uninstall it, and "which of these are already
// here" (Invoke-WinUtilCurrentSystem's winget and choco branches).
//
// Kept faithful on purpose rather than rewritten around QProcess calls to winget.exe
// directly: the value of WinUtil's install tab is a decade of small decisions — the exact
// winget switches, the msstore: prefix, the Edge stub that has to exist before Edge can
// be uninstalled, the regex that finds an id in `winget list` output — and every one of
// those is easier to keep right by keeping the text than by translating it.
//
// The script talks back with tokens rather than sentences, the same convention the
// actions and the debloat page use: `ARB-APP|start|i|n|id`, `ARB-APP|done|i|n|id`,
// `ARB-LOG|level|component|message` for WinUtil's own log lines, `ARB-PM|winget|installed`
// for the probe, and a raw block between `ARB-LIST-BEGIN` and `ARB-LIST-END` for the
// installed-program listing, which is matched here in C++ with WinUtil's own pattern.

#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QProcess;

namespace Apps {

/// One row of applications.json, exactly as WinUtil carries it.
struct Entry
{
    QString key;          ///< the JSON key, e.g. "brave"; WinUtil's checkbox is "WPFInstall" + key
    QString category;     ///< WinUtil's English category name, e.g. "Microsoft Tools"
    QString name;         ///< `content`
    QString description;  ///< `description`, WinUtil's own English text
    QString link;
    QString winget;       ///< "Brave.Brave", "msstore:9NT1R1C2HH7J", or "na"
    QString choco;        ///< "brave", "na", or empty for a row that carries none
    bool foss = false;

    bool hasWinget() const;
    bool hasChoco() const;

    /// The key a WinUtil preset file names this app by.
    QString presetKey() const { return QStringLiteral("WPFInstall") + key; }
};

/// "Microsoft Tools" → "microsoft-tools": the id the category's i18n key and its filter
/// chip are built from.
QString categorySlug(const QString &category);

/// The i18n key of a category's heading: "apps.category.<slug>".
QString categoryKey(const QString &category);

class Catalogue
{
public:
    /// Parses :/data/applications.json once. Empty on failure, never fatal.
    static const Catalogue &instance();

    /// Every entry, by category (alphabetical, as WinUtil sorts them) and then by key.
    const QVector<Entry> &entries() const { return m_entries; }
    const Entry *entry(const QString &key) const;

    /// The distinct categories, alphabetical.
    const QStringList &categories() const { return m_categories; }

    int count() const { return int(m_entries.size()); }

    /// The entry key a preset token names — "WPFInstallbrave" or plain "brave" — or an
    /// empty string for one this catalogue does not carry.
    QString keyFromPresetToken(const QString &token) const;

private:
    Catalogue();

    QVector<Entry> m_entries;
    QStringList m_categories;
    QHash<QString, int> m_index;
};

enum class Manager { Winget, Choco };

QString managerToString(Manager m);      ///< "Winget" / "Choco", WinUtil's own spellings
Manager managerFromString(const QString &s);

enum class Operation { Install, Uninstall };

/// What Get-WinUtilSelectedPackages answers: which id goes to which manager, given the
/// user's preference. Under Choco a row without a choco id falls back to its winget id;
/// under Winget every row goes to winget. "na" and empty ids are dropped, duplicates too.
struct Split
{
    QStringList winget;
    QStringList choco;

    int total() const { return int(winget.size() + choco.size()); }
    bool isEmpty() const { return winget.isEmpty() && choco.isEmpty(); }
};

Split split(const QVector<Entry> &packages, Manager preference);

/// The exact command lines the run will issue, one per line, for the confirmation dialog
/// — the same promise the actions page makes: what you read is what runs.
QString commandSummary(const Split &s, Operation op);

/// Reads and writes WinUtil's preset format: a JSON array of checkbox keys
/// ("WPFInstallbrave", …). Only the install keys are written; on import every token is
/// resolved through the catalogue, and the ones it does not know are counted rather than
/// failing the file. A legacy WinUtil export (an object with a "WPFInstall" array) is
/// read too.
bool exportSelection(const QString &path, const QStringList &keys, QString *error);
bool importSelection(const QString &path, QStringList *keys, int *unknown, QString *error);

/// One PowerShell at a time, exactly like ActionEngine, with WinUtil's functions in it.
class Runner : public QObject
{
    Q_OBJECT

public:
    enum class Job { None, Install, Uninstall, Detect, Repair, Probe };
    Q_ENUM(Job)

    explicit Runner(QObject *parent = nullptr);

    bool running() const { return m_process != nullptr; }
    Job job() const { return m_job; }

    /// Invoke-WPFInstall: Install-WinUtilWinget first when there is anything for it, then
    /// one Install-WinUtilProgramWinget per id; Install-WinUtilChoco, then one
    /// Install-WinUtilProgramChoco for the whole choco list.
    void install(const Split &s);

    /// Invoke-WPFUnInstall, including the MicrosoftEdge.exe stub it creates first.
    void uninstall(const Split &s);

    /// Invoke-WinUtilCurrentSystem -CheckBox winget|choco: lists what the manager reports
    /// installed and answers with the catalogue keys that match, through detected().
    void detectInstalled(Manager m);

    /// Invoke-WPFFixesWinget (Install-WinUtilWinget) or Install-WinUtilChoco.
    void repair(Manager m);

    /// Test-WinUtilPackageManager for both managers; answers through probed().
    void probe();

    /// Invoke-WPFInstallUpgrade: opens a PowerShell window of its own and leaves it there
    /// — `winget upgrade --all …` can take an hour, and WinUtil deliberately does not
    /// hold its own window for it. Returns false with \a error when nothing could start.
    bool upgradeAll(Manager m, QString *error);

Q_SIGNALS:
    void started(Apps::Runner::Job job, int total);
    /// \a done packages are complete out of \a total; \a id is the package this line is
    /// about and \a finished whether it just ended (else it just began). \a exitCode is
    /// the manager's, meaningful only when \a finished.
    void progress(int done, int total, const QString &id, bool finished, int exitCode);
    /// One decoded line of output, as it arrives.
    void line(const QString &text);
    void finished(Apps::Runner::Job job, bool ok, int failures);
    void detected(const QStringList &keys);
    void probed(bool winget, bool choco);

private:
    void start(Job job, const QString &body, int total);
    void consume();
    void handleLine(const QString &text);
    void onFinished(int exitCode);

    QProcess *m_process = nullptr;
    Job m_job = Job::None;
    Manager m_detectManager = Manager::Winget;
    int m_total = 0;
    int m_done = 0;
    int m_failures = 0;
    int m_lastExitCode = 0;   ///< from the last "(exit code: N)" log line, read at done
    bool m_listing = false;
    bool m_wingetPresent = false;
    bool m_chocoPresent = false;
    QString m_carry;       ///< an unterminated last line between reads
    QString m_listText;    ///< the ARB-LIST block, for detectInstalled()
    QString m_lastPackage;
    QString m_scriptPath;
    QString m_logPath;
    QString m_transcript;
};

} // namespace Apps
