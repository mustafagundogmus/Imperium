// cleaner.h — what can be reclaimed from the disk, measured and removed off the GUI thread.
//
// The Disk cleaner page is a list of targets: places Windows and the programs on it leave
// things that are safe to throw away — temp folders, the update download cache, crash
// dumps, error reports, browser and shader caches, the recycle bin, and a few heavier ones
// (previous Windows installations, old restore points, the component store) that are kept
// apart and unchecked because they cost something. Each target is measured first, so the
// page can say what a clean would actually free before anything is deleted, and then
// cleaned on request.
//
// Both halves run on a worker thread. A temp folder can hold a hundred thousand files and
// Windows.old thirty gigabytes; walking either on the GUI thread would freeze the window
// for as long as it took, and this application already had one launch that looked hung.
// The worker touches the filesystem and nothing else — every result reaches the page as a
// queued signal carrying plain values — so nothing here shares state across threads.
//
// What is not here, on purpose: the thumbnail cache, which Explorer holds open (the icon
// cache action restarts the shell for that); the hibernation file, which is a tweak; and
// anything under Downloads or Documents, which is the user's, not Windows'.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QThread;

namespace Cleaner {

enum class Kind
{
    Files,                  ///< paths under `patterns`, deleted through the filesystem
    RecycleBin,             ///< SHQueryRecycleBin / SHEmptyRecycleBin
    DeliveryOptimization,   ///< measured as files, emptied through the DO cmdlet
    ShadowCopies,           ///< every restore point but the newest, through CIM
    ComponentStore          ///< DISM's StartComponentCleanup; cannot be measured cheaply
};

enum class Group { System, User, Apps, Advanced };

struct Target
{
    QString id;                     ///< "user-temp"; the i18n keys are cleaner.<id>.name/.desc
    Kind kind = Kind::Files;
    Group group = Group::System;

    /// Paths, with %VAR% references. A pattern ending in `\*` means the *contents* of
    /// the folder go and the folder stays; one that names a folder outright means the
    /// folder itself goes. Segments may carry wildcards ("User Data\*\Cache").
    QStringList patterns;

    /// Files modified within this many days are left alone — an installer's half-written
    /// temp file is not rubbish yet. 0 takes everything.
    int keepDays = 0;

    bool defaultOn = true;          ///< checked when the page opens
    QString noteKey;                ///< i18n key of the consequence, shown on the row
    bool takeOwnership = false;     ///< TrustedInstaller owns it; takeown/icacls first

    QString nameKey() const { return QStringLiteral("cleaner.") + id + QStringLiteral(".name"); }
    QString descKey() const { return QStringLiteral("cleaner.") + id + QStringLiteral(".desc"); }
};

/// Every target, in page order.
const QVector<Target> &targets();
const Target *target(const QString &id);

/// The i18n key of a group's section heading.
QString groupKey(Group group);

/// The worker. Lives on Engine's thread; the page never talks to it directly.
class Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject *parent = nullptr) : QObject(parent) {}

    void scan();
    void clean(const QStringList &ids);

Q_SIGNALS:
    /// \a bytes is -1 for a target that cannot be measured.
    void measured(const QString &id, qint64 bytes, qint64 files);
    void scanFinished();
    void cleaned(const QString &id, qint64 freed, qint64 skipped, const QString &error);
    void cleanFinished(qint64 freed);
};

/// The GUI-side handle: owns the thread, forwards the worker's signals, and knows whether
/// anything is in flight.
class Engine : public QObject
{
    Q_OBJECT

public:
    explicit Engine(QObject *parent = nullptr);
    ~Engine() override;

    bool scanning() const { return m_scanning; }
    bool cleaning() const { return m_cleaning; }
    bool busy() const { return m_scanning || m_cleaning; }

    /// Measures every target. Does nothing while a scan or a clean is running.
    void scan();
    /// Cleans the targets named by \a ids, in page order. Does nothing while busy.
    void clean(const QStringList &ids);

Q_SIGNALS:
    void measured(const QString &id, qint64 bytes, qint64 files);
    void scanFinished();
    void cleaned(const QString &id, qint64 freed, qint64 skipped, const QString &error);
    void cleanFinished(qint64 freed);

private:
    QThread *m_thread = nullptr;
    Worker *m_worker = nullptr;
    bool m_scanning = false;
    bool m_cleaning = false;
};

} // namespace Cleaner
