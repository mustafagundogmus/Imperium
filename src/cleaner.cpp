#include "cleaner.h"
#include "console.h"
#include "winpaths.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#endif

namespace Cleaner {

// ------------------------------------------------------------------------------ targets

const QVector<Target> &targets()
{
    static const QVector<Target> list = [] {
        QVector<Target> t;
        auto add = [&t](const char *id, Kind kind, Group group, QStringList patterns,
                        int keepDays, bool on, const char *noteKey = nullptr,
                        bool takeOwnership = false) {
            Target target;
            target.id = QString::fromLatin1(id);
            target.kind = kind;
            target.group = group;
            target.patterns = std::move(patterns);
            target.keepDays = keepDays;
            target.defaultOn = on;
            if (noteKey)
                target.noteKey = QString::fromLatin1(noteKey);
            target.takeOwnership = takeOwnership;
            t.append(target);
        };
        using S = QString;

        // --- Windows --------------------------------------------------------------
        add("windows-temp", Kind::Files, Group::System,
            {S("%SystemRoot%\\Temp\\*")}, 1, true, "cleaner.note.keepDay");
        add("update-cache", Kind::Files, Group::System,
            {S("%SystemRoot%\\SoftwareDistribution\\Download\\*")}, 0, true,
            "cleaner.note.updateCache");
        add("delivery-optimization", Kind::DeliveryOptimization, Group::System,
            {S("%SystemRoot%\\ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft"
               "\\Windows\\DeliveryOptimization\\Cache\\*")}, 0, true);
        add("windows-logs", Kind::Files, Group::System,
            {S("%SystemRoot%\\Logs\\CBS\\*"), S("%SystemRoot%\\Logs\\DISM\\*"),
             S("%SystemRoot%\\Logs\\MoSetup\\*"), S("%SystemRoot%\\Logs\\WindowsUpdate\\*")},
            0, true);
        add("crash-dumps", Kind::Files, Group::System,
            {S("%SystemRoot%\\Minidump\\*"), S("%SystemRoot%\\MEMORY.DMP"),
             S("%SystemRoot%\\LiveKernelReports\\*"), S("%LocalAppData%\\CrashDumps\\*")},
            0, true, "cleaner.note.dumps");
        add("error-reports", Kind::Files, Group::System,
            {S("%ProgramData%\\Microsoft\\Windows\\WER\\ReportQueue\\*"),
             S("%ProgramData%\\Microsoft\\Windows\\WER\\ReportArchive\\*"),
             S("%ProgramData%\\Microsoft\\Windows\\WER\\Temp\\*"),
             S("%LocalAppData%\\Microsoft\\Windows\\WER\\ReportQueue\\*"),
             S("%LocalAppData%\\Microsoft\\Windows\\WER\\ReportArchive\\*")},
            0, true);

        // --- this user ------------------------------------------------------------
        add("user-temp", Kind::Files, Group::User,
            {S("%TEMP%\\*")}, 1, true, "cleaner.note.keepDay");
        add("inet-cache", Kind::Files, Group::User,
            {S("%LocalAppData%\\Microsoft\\Windows\\INetCache\\*")}, 0, true);
        add("recycle-bin", Kind::RecycleBin, Group::User, {}, 0, false,
            "cleaner.note.recycleBin");

        // --- programs -------------------------------------------------------------
        add("browser-cache", Kind::Files, Group::Apps,
            {S("%LocalAppData%\\Microsoft\\Edge\\User Data\\*\\Cache\\*"),
             S("%LocalAppData%\\Microsoft\\Edge\\User Data\\*\\Code Cache\\*"),
             S("%LocalAppData%\\Microsoft\\Edge\\User Data\\*\\GPUCache\\*"),
             S("%LocalAppData%\\Google\\Chrome\\User Data\\*\\Cache\\*"),
             S("%LocalAppData%\\Google\\Chrome\\User Data\\*\\Code Cache\\*"),
             S("%LocalAppData%\\Google\\Chrome\\User Data\\*\\GPUCache\\*"),
             S("%LocalAppData%\\BraveSoftware\\Brave-Browser\\User Data\\*\\Cache\\*"),
             S("%LocalAppData%\\BraveSoftware\\Brave-Browser\\User Data\\*\\Code Cache\\*"),
             S("%LocalAppData%\\Vivaldi\\User Data\\*\\Cache\\*"),
             S("%LocalAppData%\\Opera Software\\Opera Stable\\Cache\\*"),
             S("%LocalAppData%\\Mozilla\\Firefox\\Profiles\\*\\cache2\\*")},
            0, true, "cleaner.note.browser");
        add("shader-cache", Kind::Files, Group::Apps,
            {S("%LocalAppData%\\NVIDIA\\DXCache\\*"), S("%LocalAppData%\\NVIDIA\\GLCache\\*"),
             S("%LocalAppData%\\AMD\\DxCache\\*"), S("%LocalAppData%\\AMD\\GLCache\\*"),
             S("%LocalAppData%\\D3DSCache\\*"), S("%LocalAppData%\\Intel\\ShaderCache\\*")},
            0, true, "cleaner.note.shader");
        add("app-caches", Kind::Files, Group::Apps,
            {S("%AppData%\\discord\\Cache\\*"), S("%AppData%\\discord\\Code Cache\\*"),
             S("%AppData%\\discord\\GPUCache\\*"),
             S("%AppData%\\Microsoft\\Teams\\Cache\\*"),
             S("%AppData%\\Microsoft\\Teams\\Code Cache\\*"),
             S("%LocalAppData%\\Steam\\htmlcache\\*"),
             S("%AppData%\\Spotify\\Storage\\*"),
             S("%LocalAppData%\\Packages\\Microsoft.WindowsStore_8wekyb3d8bbwe\\LocalCache\\*")},
            0, true, "cleaner.note.browser");

        // --- with a cost ----------------------------------------------------------
        add("prefetch", Kind::Files, Group::Advanced,
            {S("%SystemRoot%\\Prefetch\\*")}, 0, false, "cleaner.note.prefetch");
        add("upgrade-leftovers", Kind::Files, Group::Advanced,
            {S("%SystemDrive%\\Windows.old"), S("%SystemDrive%\\$Windows.~BT"),
             S("%SystemDrive%\\$Windows.~WS"), S("%SystemDrive%\\$GetCurrent"),
             S("%SystemDrive%\\$SysReset")},
            0, false, "cleaner.note.windowsOld", true);
        add("shadow-copies", Kind::ShadowCopies, Group::Advanced, {}, 0, false,
            "cleaner.note.shadow");
        add("component-store", Kind::ComponentStore, Group::Advanced, {}, 0, false,
            "cleaner.note.componentStore");
        return t;
    }();
    return list;
}

const Target *target(const QString &id)
{
    for (const Target &t : targets())
        if (t.id == id)
            return &t;
    return nullptr;
}

QString groupKey(Group group)
{
    switch (group) {
    case Group::System:   return QStringLiteral("cleaner.section.system");
    case Group::User:     return QStringLiteral("cleaner.section.user");
    case Group::Apps:     return QStringLiteral("cleaner.section.apps");
    case Group::Advanced: break;
    }
    return QStringLiteral("cleaner.section.advanced");
}

// ------------------------------------------------------------------------------ helpers

namespace {

bool interrupted()
{
    return QThread::currentThread()->isInterruptionRequested();
}

/// %VAR% references resolved. SystemRoot and SystemDrive come from the API, like every
/// other Windows directory in this application; the per-user ones can only come from the
/// environment, which is this user's own — the elevated token does not change who that is.
/// An empty string means a variable the machine does not define, and the pattern is
/// skipped rather than turned into a relative path somewhere unexpected.
QString expandVars(const QString &pattern)
{
    QString s = pattern;
    s.replace(QLatin1String("%SystemRoot%"), WinPaths::windows(), Qt::CaseInsensitive);
    s.replace(QLatin1String("%SystemDrive%"), WinPaths::windows().left(2), Qt::CaseInsensitive);

    static const QRegularExpression var(QStringLiteral("%([A-Za-z0-9_()]+)%"));
    QRegularExpressionMatch m;
    while ((m = var.match(s)).hasMatch()) {
        const QString value = qEnvironmentVariable(m.captured(1).toLatin1().constData());
        if (value.isEmpty())
            return QString();
        s.replace(m.capturedStart(0), m.capturedLength(0), value);
    }
    return QDir::fromNativeSeparators(s);
}

bool hasWildcard(const QString &segment)
{
    return segment.contains(QLatin1Char('*')) || segment.contains(QLatin1Char('?'));
}

/// The existing paths a pattern names. Segments with wildcards are matched against the
/// directory they sit in; a final `*` lists the folder's contents, which is what "the
/// contents go, the folder stays" means downstream.
QStringList expand(const QString &pattern)
{
    const QString expanded = expandVars(pattern);
    if (expanded.isEmpty())
        return {};

    QStringList parts = expanded.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};

    QStringList current{parts.takeFirst() + QLatin1Char('/')};   // "C:/"
    for (int i = 0; i < parts.size() && !current.isEmpty(); ++i) {
        const QString &segment = parts.at(i);
        const bool last = i == parts.size() - 1;
        QStringList next;
        for (const QString &base : std::as_const(current)) {
            if (hasWildcard(segment)) {
                QDir dir(base);
                const QDir::Filters filters = (last ? QDir::Files | QDir::Dirs : QDir::Dirs)
                                              | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot;
                const QStringList entries = dir.entryList({segment}, filters);
                for (const QString &entry : entries)
                    next << dir.filePath(entry);
            } else {
                const QString candidate = QDir(base).filePath(segment);
                if (QFileInfo::exists(candidate))
                    next << candidate;
            }
        }
        current = next;
    }
    return current;
}

struct Tally
{
    qint64 bytes = 0;
    qint64 files = 0;
    qint64 skipped = 0;
};

bool oldEnough(const QFileInfo &info, int keepDays)
{
    if (keepDays <= 0)
        return true;
    return info.lastModified() < QDateTime::currentDateTime().addDays(-keepDays);
}

/// Sizes everything under \a path that a clean would remove. Symlinks and junctions are
/// counted as nothing and never followed: a junction out of a temp folder into a real one
/// is exactly the shape a size walk must not go down.
void measurePath(const QString &path, int keepDays, Tally *tally)
{
    const QFileInfo info(path);
    if (info.isSymLink())
        return;
    if (info.isFile()) {
        if (oldEnough(info, keepDays)) {
            tally->bytes += info.size();
            ++tally->files;
        }
        return;
    }
    if (!info.isDir())
        return;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (interrupted())
            return;
        it.next();
        const QFileInfo f = it.fileInfo();
        if (f.isSymLink() || !oldEnough(f, keepDays))
            continue;
        tally->bytes += f.size();
        ++tally->files;
    }
}

void removeFile(const QFileInfo &info, Tally *tally)
{
    const qint64 size = info.size();
    QFile file(info.filePath());
    if (file.remove()) {
        tally->bytes += size;
        ++tally->files;
        return;
    }
    // Read-only is not "in use". Clear it and try once more; anything that still
    // refuses is open somewhere, and is counted rather than fought over.
    file.setPermissions(file.permissions() | QFile::WriteOwner | QFile::WriteUser);
    if (file.remove()) {
        tally->bytes += size;
        ++tally->files;
        return;
    }
    ++tally->skipped;
}

/// Removes what measurePath() counted, then the folders that emptied out, bottom-up.
/// \a self says whether \a path itself goes when it is empty.
void removePath(const QString &path, int keepDays, bool self, Tally *tally)
{
    const QFileInfo info(path);
    if (info.isSymLink()) {
        // A junction or a symlink at the top level: unlink the link, never its target.
        if (self && !QFile::remove(path))
            QDir().rmdir(path);
        return;
    }
    if (info.isFile()) {
        if (oldEnough(info, keepDays))
            removeFile(info, tally);
        return;
    }
    if (!info.isDir())
        return;

    QStringList dirs;
    QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (interrupted())
            return;
        it.next();
        const QFileInfo f = it.fileInfo();
        if (f.isSymLink()) {
            // Inside the tree: unlink, do not descend. QDirIterator already does not
            // follow it, so all that is left is the entry itself.
            if (!QFile::remove(f.filePath()))
                QDir().rmdir(f.filePath());
            continue;
        }
        if (f.isDir()) {
            dirs << f.filePath();
            continue;
        }
        if (oldEnough(f, keepDays))
            removeFile(f, tally);
    }

    // Deepest first, so a parent is only tried after its children have had their turn.
    // rmdir refuses a folder that still holds anything, which is what keeps a folder
    // that lost only some of its files.
    std::sort(dirs.begin(), dirs.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    for (const QString &dir : std::as_const(dirs))
        QDir().rmdir(dir);
    if (self)
        QDir().rmdir(path);
}

/// The full path of one of the Windows console tools, from System32 and nowhere else.
QString tool(const char *name)
{
    return WinPaths::system32() + QLatin1Char('\\') + QString::fromLatin1(name);
}

/// Runs a program to completion in this (worker) thread and returns its exit code, -1 when
/// it could not be started. Output is collected for the caller that wants it.
int run(const QString &program, const QStringList &arguments, QString *output = nullptr,
        int timeoutMs = 30 * 60 * 1000)
{
    if (program.isEmpty() || !QFileInfo::exists(program))
        return -1;
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    process.start();
    if (!process.waitForStarted(10000))
        return -1;
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(5000);
        return -1;
    }
    if (output)
        *output = Console::decode(process.readAll()).trimmed();
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

/// Runs a PowerShell script through a file, the way ActionEngine does — a -Command line
/// would have to survive two rounds of quoting — and returns what it printed.
int runPowerShell(const QString &script, QString *output)
{
    const QString powershell = WinPaths::powershell();
    if (powershell.isEmpty())
        return -1;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/cleaner.ps1");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return -1;
    file.write("\xEF\xBB\xBF");
    file.write("$ErrorActionPreference = 'Stop'\n");
    file.write(script.toUtf8());
    file.write("\n");
    file.close();
    return run(powershell,
               {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                QStringLiteral("-File"), QDir::toNativeSeparators(path)},
               output);
}

/// "bytes|count" for the shadow copies on the machine.
bool measureShadows(qint64 *bytes, qint64 *count)
{
    QString out;
    const int code = runPowerShell(QStringLiteral(
        "$used = (Get-CimInstance Win32_ShadowStorage -ErrorAction SilentlyContinue | "
        "Measure-Object -Property UsedSpace -Sum).Sum\n"
        "$n = @(Get-CimInstance Win32_ShadowCopy -ErrorAction SilentlyContinue).Count\n"
        "Write-Output ('{0}|{1}' -f [int64]$used, [int64]$n)"), &out);
    if (code != 0)
        return false;
    const QStringList parts = out.section(QLatin1Char('\n'), -1).split(QLatin1Char('|'));
    if (parts.size() != 2)
        return false;
    *bytes = parts.at(0).trimmed().toLongLong();
    *count = parts.at(1).trimmed().toLongLong();
    return true;
}

Tally measureFiles(const Target &t)
{
    Tally tally;
    for (const QString &pattern : t.patterns) {
        const QStringList paths = expand(pattern);
        for (const QString &path : paths) {
            if (interrupted())
                return tally;
            measurePath(path, t.keepDays, &tally);
        }
    }
    return tally;
}

#ifdef Q_OS_WIN
bool measureRecycleBin(qint64 *bytes, qint64 *items)
{
    SHQUERYRBINFO info{};
    info.cbSize = sizeof(info);
    if (FAILED(SHQueryRecycleBinW(nullptr, &info)))
        return false;
    *bytes = qint64(info.i64Size);
    *items = qint64(info.i64NumItems);
    return true;
}
#endif

} // namespace

// ------------------------------------------------------------------------------- worker

void Worker::scan()
{
    for (const Target &t : targets()) {
        if (interrupted())
            break;
        switch (t.kind) {
        case Kind::Files:
        case Kind::DeliveryOptimization: {
            const Tally tally = measureFiles(t);
            Q_EMIT measured(t.id, tally.bytes, tally.files);
            break;
        }
        case Kind::RecycleBin: {
            qint64 bytes = 0;
            qint64 items = 0;
#ifdef Q_OS_WIN
            if (measureRecycleBin(&bytes, &items))
                Q_EMIT measured(t.id, bytes, items);
            else
#endif
                Q_EMIT measured(t.id, -1, 0);
            break;
        }
        case Kind::ShadowCopies: {
            qint64 bytes = 0;
            qint64 count = 0;
            if (measureShadows(&bytes, &count))
                // What a clean frees is every copy but the newest; the storage figure is
                // for all of them, and CIM does not split it, so the count is the honest
                // number and the size an upper bound.
                Q_EMIT measured(t.id, count > 1 ? bytes : 0, qMax<qint64>(0, count - 1));
            else
                Q_EMIT measured(t.id, -1, 0);
            break;
        }
        case Kind::ComponentStore:
            // DISM's AnalyzeComponentStore takes a minute and answers in the display
            // language; the row says "cannot be measured" rather than guessing.
            Q_EMIT measured(t.id, -1, 0);
            break;
        }
    }
    Q_EMIT scanFinished();
}

void Worker::clean(const QStringList &ids)
{
    qint64 total = 0;
    for (const Target &t : targets()) {
        if (!ids.contains(t.id))
            continue;
        if (interrupted())
            break;

        Tally tally;
        QString error;
        switch (t.kind) {
        case Kind::Files: {
            for (const QString &pattern : t.patterns) {
                const bool contents = pattern.endsWith(QLatin1String("\\*"));
                const QStringList paths = expand(pattern);
                for (const QString &path : paths) {
                    if (interrupted())
                        break;
                    if (t.takeOwnership) {
                        // Windows.old and the upgrade folders belong to TrustedInstaller
                        // and SYSTEM, with an ACL that turns an administrator away; the
                        // same two tools the take-ownership verb uses, from System32.
                        const QString native = QDir::toNativeSeparators(path);
                        run(tool("takeown.exe"), {QStringLiteral("/F"), native, QStringLiteral("/R"),
                                                  QStringLiteral("/D"), QStringLiteral("Y")});
                        run(tool("icacls.exe"), {native, QStringLiteral("/grant:r"),
                                                 QStringLiteral("*S-1-5-32-544:F"), QStringLiteral("/T"),
                                                 QStringLiteral("/C"), QStringLiteral("/Q")});
                    }
                    // A pattern that named the folder outright takes the folder with it;
                    // a `\*` pattern already expanded to the children, each of which goes
                    // whole, so `self` is true for both — the folder the `*` stood in
                    // was never in the list.
                    Q_UNUSED(contents);
                    removePath(path, t.keepDays, true, &tally);
                }
            }
            break;
        }
        case Kind::RecycleBin: {
#ifdef Q_OS_WIN
            qint64 bytes = 0;
            qint64 items = 0;
            measureRecycleBin(&bytes, &items);
            const HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr,
                                                  SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            // S_OK, or the "already empty" answer some builds give as an error code.
            if (SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || items == 0) {
                tally.bytes = bytes;
                tally.files = items;
            } else {
                error = QStringLiteral("SHEmptyRecycleBin 0x%1").arg(quint32(hr), 8, 16, QLatin1Char('0'));
            }
#endif
            break;
        }
        case Kind::DeliveryOptimization: {
            const Tally before = measureFiles(t);
            QString out;
            const int code = runPowerShell(
                QStringLiteral("Delete-DeliveryOptimizationCache -Force | Out-Null"), &out);
            if (code != 0) {
                // The cmdlet is 1709+; on anything older, or if it refuses, the cache
                // folder itself is still just files.
                for (const QString &pattern : t.patterns)
                    for (const QString &path : expand(pattern))
                        removePath(path, 0, true, &tally);
            }
            const Tally after = measureFiles(t);
            tally.bytes = qMax<qint64>(0, before.bytes - after.bytes);
            tally.files = qMax<qint64>(0, before.files - after.files);
            tally.skipped = after.files;
            break;
        }
        case Kind::ShadowCopies: {
            qint64 before = 0;
            qint64 count = 0;
            measureShadows(&before, &count);
            QString out;
            const int code = runPowerShell(QStringLiteral(
                "Get-CimInstance Win32_ShadowCopy | Sort-Object InstallDate | "
                "Select-Object -SkipLast 1 | Remove-CimInstance"), &out);
            if (code != 0) {
                error = out.section(QLatin1Char('\n'), 0, 0);
            } else {
                qint64 after = 0;
                qint64 left = 0;
                measureShadows(&after, &left);
                tally.bytes = qMax<qint64>(0, before - after);
                tally.files = qMax<qint64>(0, count - left);
            }
            break;
        }
        case Kind::ComponentStore: {
            QString out;
            const int code = run(tool("Dism.exe"),
                                 {QStringLiteral("/Online"), QStringLiteral("/Cleanup-Image"),
                                  QStringLiteral("/StartComponentCleanup")},
                                 &out);
            // 3010 is "done, restart to finish", which is success for this purpose.
            if (code != 0 && code != 3010)
                error = QStringLiteral("DISM %1").arg(code);
            break;
        }
        }

        total += tally.bytes;
        Q_EMIT cleaned(t.id, tally.bytes, tally.skipped, error);
    }
    Q_EMIT cleanFinished(total);
}

// ------------------------------------------------------------------------------- engine

Engine::Engine(QObject *parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_worker(new Worker)
{
    m_thread->setObjectName(QStringLiteral("cleaner"));
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Cross-thread, so every one of these is queued and lands on the GUI thread.
    connect(m_worker, &Worker::measured, this, &Engine::measured);
    connect(m_worker, &Worker::cleaned, this, &Engine::cleaned);
    connect(m_worker, &Worker::scanFinished, this, [this] {
        m_scanning = false;
        Q_EMIT scanFinished();
    });
    connect(m_worker, &Worker::cleanFinished, this, [this](qint64 freed) {
        m_cleaning = false;
        Q_EMIT cleanFinished(freed);
    });

    m_thread->start();
}

Engine::~Engine()
{
    // A clean in the middle of DISM cannot be interrupted quickly; give it a moment and
    // then let the process end rather than hold the exit for the whole of it.
    m_thread->requestInterruption();
    m_thread->quit();
    if (!m_thread->wait(15000))
        m_thread->terminate();
}

void Engine::scan()
{
    if (busy())
        return;
    m_scanning = true;
    Worker *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker] { worker->scan(); }, Qt::QueuedConnection);
}

void Engine::clean(const QStringList &ids)
{
    if (busy() || ids.isEmpty())
        return;
    m_cleaning = true;
    Worker *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, ids] { worker->clean(ids); }, Qt::QueuedConnection);
}

} // namespace Cleaner
