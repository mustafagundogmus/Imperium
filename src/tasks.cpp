#include "tasks.h"
#include "i18n.h"

#include <QCollator>

#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <objbase.h>
#  include <oleauto.h>
#  include <shlwapi.h>
#  include <taskschd.h>
#endif

namespace Tasks {
namespace {

#ifdef Q_OS_WIN

// The two GUIDs the scheduler is reached through, spelled out rather than taken from
// uuid.lib: MinGW's copy of that library has not carried every scheduler GUID in every
// release, and a link error over a constant is not worth a build's time.
const CLSID ClsidTaskScheduler = {0x0F87369F, 0xA4E5, 0x4CFC, {0xBD, 0x3E, 0x73, 0xE6, 0x15, 0x45, 0x72, 0xDD}};
const IID IidTaskService = {0x2FABA4C7, 0x4DA9, 0x4013, {0x96, 0x97, 0x20, 0xCC, 0x3F, 0xD4, 0x0F, 0x85}};

/// Releases a COM interface when it goes out of scope.
template <typename T>
struct Ptr
{
    T *p = nullptr;
    ~Ptr() { if (p) p->Release(); }
    T **operator&() { return &p; }
    T *operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
    Ptr() = default;
    Ptr(const Ptr &) = delete;
    Ptr &operator=(const Ptr &) = delete;
};

/// A BSTR that frees itself, and reads as a QString with its real length: a BSTR may hold
/// embedded nulls, and fromWCharArray without a length would stop at the first one.
struct Bstr
{
    BSTR b = nullptr;
    ~Bstr() { SysFreeString(b); }
    QString toString() const { return b ? QString::fromWCharArray(b, int(SysStringLen(b))) : QString(); }
    Bstr() = default;
    Bstr(const Bstr &) = delete;
    Bstr &operator=(const Bstr &) = delete;
};

BSTR bstr(const QString &s)
{
    return SysAllocStringLen(reinterpret_cast<const OLECHAR *>(s.utf16()), UINT(s.size()));
}

/// Windows' own words for an HRESULT, or the code when it has none.
QString describe(HRESULT hr)
{
    if (hr == E_ACCESSDENIED)
        return Locale::tr(QStringLiteral("task.err.accessDenied"));
    LPWSTR buffer = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                       | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, DWORD(hr), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString text = (n && buffer) ? QString::fromWCharArray(buffer, int(n)).trimmed() : QString();
    if (buffer)
        LocalFree(buffer);
    if (text.isEmpty())
        text = QStringLiteral("HRESULT 0x%1").arg(quint32(hr), 8, 16, QLatin1Char('0'));
    return text;
}

/// One connection to the scheduler for the life of the process.
///
/// Connect() is the expensive call — it authenticates against the service — and the
/// engine asks after every task's state at startup and again after every write, several
/// hundred times a session. One connection, made on the GUI thread the first time it is
/// needed and kept, turns that into one authentication. The apartment it lives in is the
/// one QApplication initialised for this thread and keeps until exit; the CoInitializeEx
/// here only adds a count to it, and is deliberately never balanced, because the pointer
/// it protects is never released before the process ends.
ITaskService *service()
{
    static ITaskService *shared = [] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ITaskService *svc = nullptr;
        if (FAILED(CoCreateInstance(ClsidTaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IidTaskService,
                                    reinterpret_cast<void **>(&svc))))
            return static_cast<ITaskService *>(nullptr);
        VARIANT empty;
        VariantInit(&empty);
        if (FAILED(svc->Connect(empty, empty, empty, empty))) {
            svc->Release();
            return static_cast<ITaskService *>(nullptr);
        }
        return svc;
    }();
    return shared;
}

/// A task's description is usually "$(@%SystemRoot%\system32\x.dll,-123)": a pointer into
/// a resource table, wrapped in the MUI form. Unwrapped, expanded and resolved into the
/// system's language; anything that is not that shape is already text.
QString resolveDescription(const QString &raw)
{
    QString text = raw.trimmed();
    if (text.startsWith(QLatin1String("$(")) && text.endsWith(QLatin1Char(')')))
        text = text.mid(2, text.size() - 3);
    if (!text.startsWith(QLatin1Char('@')))
        return text;

    wchar_t expanded[2048] = {};
    if (!ExpandEnvironmentStringsW(reinterpret_cast<const wchar_t *>(text.utf16()), expanded,
                                   ARRAYSIZE(expanded)))
        return {};
    wchar_t out[2048] = {};
    if (SUCCEEDED(SHLoadIndirectString(expanded, out, ARRAYSIZE(out), nullptr)))
        return QString::fromWCharArray(out).trimmed();
    return {};
}

/// The folders whose tasks carry a note or a lock, matched on the folder path or any
/// folder beneath it.
///
/// Locks are for the tasks Windows protects with a DACL that turns away an administrator
/// token — the update orchestrator's since 1903, and Scheduled Start beside it. The switch
/// would only ever fail; saying so on the row is better than a failed apply. The rest are
/// warnings: disabling them is allowed and each one costs something a user should hear
/// about before, not after.
struct FolderRule
{
    const wchar_t *folder;
    const char *riskKey;
    bool lock;
};

const FolderRule FolderRules[] = {
    {L"\\Microsoft\\Windows\\UpdateOrchestrator", "task.risk.update", true},
    {L"\\Microsoft\\Windows\\WindowsUpdate", "task.risk.update", true},
    {L"\\Microsoft\\Windows\\Windows Defender", "task.risk.defender", false},
    {L"\\Microsoft\\Windows\\SystemRestore", "task.risk.restore", false},
    {L"\\Microsoft\\Windows\\Defrag", "task.risk.defrag", false},
    {L"\\Microsoft\\Windows\\Chkdsk", "task.risk.chkdsk", false},
    {L"\\Microsoft\\Windows\\TaskScheduler", "task.risk.maintenance", false},
    {L"\\Microsoft\\Windows\\Time Synchronization", "task.risk.time", false},
    {L"\\Microsoft\\Windows\\Servicing", "task.risk.servicing", false},
    {L"\\Microsoft\\Windows\\DiskCleanup", "task.risk.diskCleanup", false},
};

bool underFolder(const QString &folder, const wchar_t *prefix)
{
    const QString p = QString::fromWCharArray(prefix);
    if (!folder.startsWith(p, Qt::CaseInsensitive))
        return false;
    return folder.size() == p.size() || folder.at(p.size()) == QLatin1Char('\\');
}

void walk(ITaskFolder *folder, QVector<Info> *out)
{
    Ptr<IRegisteredTaskCollection> tasks;
    if (SUCCEEDED(folder->GetTasks(TASK_ENUM_HIDDEN, &tasks)) && tasks) {
        LONG count = 0;
        tasks->get_Count(&count);
        for (LONG i = 1; i <= count; ++i) {   // collections are 1-based
            VARIANT index;
            VariantInit(&index);
            index.vt = VT_I4;
            index.lVal = i;
            Ptr<IRegisteredTask> task;
            if (FAILED(tasks->get_Item(index, &task)) || !task)
                continue;

            Info info;
            Bstr path;
            Bstr name;
            task->get_Path(&path.b);
            task->get_Name(&name.b);
            info.path = path.toString();
            info.name = name.toString();
            if (info.path.isEmpty())
                continue;
            const int slash = info.path.lastIndexOf(QLatin1Char('\\'));
            info.folder = slash > 0 ? info.path.left(slash) : QStringLiteral("\\");

            VARIANT_BOOL enabled = VARIANT_TRUE;
            task->get_Enabled(&enabled);
            info.enabled = enabled != VARIANT_FALSE;

            TASK_STATE state = TASK_STATE_UNKNOWN;
            if (SUCCEEDED(task->get_State(&state)))
                info.state = int(state);

            Ptr<ITaskDefinition> definition;
            if (SUCCEEDED(task->get_Definition(&definition)) && definition) {
                Ptr<IRegistrationInfo> registration;
                if (SUCCEEDED(definition->get_RegistrationInfo(&registration)) && registration) {
                    Bstr description;
                    registration->get_Description(&description.b);
                    info.description = resolveDescription(description.toString());
                }
                Ptr<ITaskSettings> settings;
                if (SUCCEEDED(definition->get_Settings(&settings)) && settings) {
                    VARIANT_BOOL hidden = VARIANT_FALSE;
                    settings->get_Hidden(&hidden);
                    info.hidden = hidden != VARIANT_FALSE;
                }
            }

            for (const FolderRule &rule : FolderRules) {
                if (!underFolder(info.folder, rule.folder))
                    continue;
                info.riskNoteKey = QString::fromLatin1(rule.riskKey);
                if (rule.lock) {
                    info.locked = true;
                    info.lockReason = Locale::tr(QStringLiteral("task.lockReason"));
                }
                break;
            }

            out->append(info);
        }
    }

    Ptr<ITaskFolderCollection> folders;
    if (SUCCEEDED(folder->GetFolders(0, &folders)) && folders) {
        LONG count = 0;
        folders->get_Count(&count);
        for (LONG i = 1; i <= count; ++i) {
            VARIANT index;
            VariantInit(&index);
            index.vt = VT_I4;
            index.lVal = i;
            Ptr<ITaskFolder> sub;
            if (SUCCEEDED(folders->get_Item(index, &sub)) && sub)
                walk(sub.p, out);
        }
    }
}

/// The task at \a path, through the root folder: GetTask takes a path relative to the
/// folder it is asked of, and the root makes an absolute one relative.
HRESULT openTask(const QString &path, IRegisteredTask **task)
{
    *task = nullptr;
    ITaskService *svc = service();
    if (!svc)
        return E_FAIL;
    Ptr<ITaskFolder> root;
    Bstr rootPath;
    rootPath.b = bstr(QStringLiteral("\\"));
    HRESULT hr = svc->GetFolder(rootPath.b, &root);
    if (FAILED(hr) || !root)
        return FAILED(hr) ? hr : E_FAIL;
    Bstr taskPath;
    taskPath.b = bstr(path);
    return root->GetTask(taskPath.b, task);
}

#endif // Q_OS_WIN

} // namespace

QString idFor(const QString &path)
{
    QString id = path;
    if (id.startsWith(QLatin1Char('\\')))
        id.remove(0, 1);
    return QStringLiteral("task-") + id.replace(QLatin1Char('\\'), QLatin1Char('|'));
}

QVector<Info> enumerate()
{
    QVector<Info> tasks;
#ifdef Q_OS_WIN
    ITaskService *svc = service();
    if (!svc)
        return tasks;

    Ptr<ITaskFolder> root;
    Bstr rootPath;
    rootPath.b = bstr(QStringLiteral("\\"));
    if (FAILED(svc->GetFolder(rootPath.b, &root)) || !root)
        return tasks;

    walk(root.p, &tasks);

    // By folder, then by name, both in the system's collation: the folders are what the
    // page groups by, and the names come back in the system's language.
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(tasks.begin(), tasks.end(), [&collator](const Info &a, const Info &b) {
        const int byFolder = collator.compare(a.folder, b.folder);
        if (byFolder != 0)
            return byFolder < 0;
        return collator.compare(a.name, b.name) < 0;
    });
#endif
    return tasks;
}

int isEnabled(const QString &path)
{
#ifdef Q_OS_WIN
    Ptr<IRegisteredTask> task;
    if (FAILED(openTask(path, &task)) || !task)
        return -1;
    VARIANT_BOOL enabled = VARIANT_TRUE;
    if (FAILED(task->get_Enabled(&enabled)))
        return -1;
    return enabled != VARIANT_FALSE ? 1 : 0;
#else
    Q_UNUSED(path);
    return -1;
#endif
}

bool setEnabled(const QString &path, bool enabled, QString *error)
{
#ifdef Q_OS_WIN
    Ptr<IRegisteredTask> task;
    HRESULT hr = openTask(path, &task);
    if (FAILED(hr) || !task) {
        if (error)
            *error = FAILED(hr) ? describe(hr) : Locale::tr(QStringLiteral("task.err.notFound"));
        return false;
    }
    hr = task->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
    if (FAILED(hr)) {
        if (error)
            *error = describe(hr);
        return false;
    }
    return true;
#else
    Q_UNUSED(path); Q_UNUSED(enabled);
    if (error)
        *error = Locale::tr(QStringLiteral("err.windowsOnly"));
    return false;
#endif
}

} // namespace Tasks
