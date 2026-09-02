// tasks.h — the machine's scheduled tasks, read and switched through the Task Scheduler.
//
// The third thing this app synthesises from the machine rather than reads from a
// catalogue, after services and startup entries. A scheduled task is a position in the
// same sense a service's start type is — enabled or disabled — but unlike those two it is
// not a registry value: its enabled flag lives in the task's own XML and the scheduler's
// cache, and the only honest way to read or write it is the Task Scheduler's COM interface.
// So a task tweak carries a RegistryEntry whose hive is the TASK pseudo-hive below and
// whose path is the task's full path, and TweakEngine routes such entries here instead of
// to Registry. The journal records the previous state as "1" or "0", which is enough to
// put a task back.
//
// Nothing here is curated. The list is every task the scheduler holds, hidden ones
// included, which is what Task Scheduler itself shows with "Show hidden tasks" on. What
// this file adds is the two things the scheduler does not say: a warning on the rows whose
// consequence is worth knowing (Defender's scans, the update tasks, System Restore), and a
// lock on the ones Windows refuses to let even an administrator touch.

#pragma once

#include <QString>
#include <QVector>

namespace Tasks {

/// The pseudo-hive a catalogue entry carries when it names a scheduled task. Compared
/// case-insensitively by isTaskEntry(); written into the journal as the previous type.
inline const QString Hive = QStringLiteral("TASK");

inline bool isTaskEntry(const QString &hive)
{
    return hive.compare(Hive, Qt::CaseInsensitive) == 0;
}

struct Info
{
    QString path;          ///< "\Microsoft\Windows\Application Experience\StartupAppTask"
    QString name;          ///< the last segment
    QString folder;        ///< everything before it; "\" for the root
    QString description;   ///< resolved out of the MUI indirection, often empty
    bool enabled = true;
    int state = 0;         ///< TASK_STATE: 0 unknown · 1 disabled · 2 queued · 3 ready · 4 running
    bool hidden = false;

    /// Windows protects a handful of its own tasks — the update orchestrator's, for one —
    /// with an ACL that refuses an administrator token. Offering a switch for those is
    /// offering a write that fails, so they are shown locked with the reason.
    bool locked = false;
    QString lockReason;

    /// An i18n key for the consequence of disabling this task, when it has one worth
    /// stating on the row. Empty for most.
    QString riskNoteKey;
};

/// Every task the scheduler holds, sorted by folder and then by name. Read-only.
QVector<Info> enumerate();

/// 1 when the task is enabled, 0 when it is disabled, -1 when it is not there or cannot
/// be read.
int isEnabled(const QString &path);

/// Flips the task's enabled flag. False and \a error on failure — an access-denied here
/// means the task is one Windows protects.
bool setEnabled(const QString &path, bool enabled, QString *error = nullptr);

/// The catalogue id for a task path: "task-" and the path with its separators replaced,
/// because an id travels through QSettings — where a backslash is a group separator —
/// as well as through preset files and the journal.
QString idFor(const QString &path);

} // namespace Tasks
