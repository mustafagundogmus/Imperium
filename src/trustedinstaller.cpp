#include "trustedinstaller.h"

#include "i18n.h"
#include "registry.h"

#include <QDir>
#include <QFileInfo>

#include <vector>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace TrustedInstaller {
namespace {

QString tr(const char *key)
{
    return Locale::tr(QString::fromLatin1(key));
}

#ifdef Q_OS_WIN

/// Windows' own words for an error code, or the hex code when it has none.
QString win32Message(DWORD code)
{
    LPWSTR buffer = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                       | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString message = (n && buffer) ? QString::fromWCharArray(buffer, int(n)).trimmed() : QString();
    if (buffer)
        LocalFree(buffer);
    if (message.isEmpty())
        message = QStringLiteral("0x%1").arg(code, 8, 16, QLatin1Char('0'));
    return QStringLiteral("%1 (%2)").arg(message).arg(code);
}

/// Turns on one privilege in this process's own token. SeDebugPrivilege is what lets us
/// open the TrustedInstaller process, which runs as SYSTEM. Best effort — an elevated
/// token holds it, and the call that needs it reports its own failure if it does not.
void enablePrivilege(const wchar_t *name)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return;
    LUID luid{};
    if (LookupPrivilegeValueW(nullptr, name, &luid)) {
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    CloseHandle(token);
}

/// Ensures the TrustedInstaller service is running and returns its process id, or 0.
///
/// The service stops itself again after a short idle, so it is started on demand rather
/// than assumed. QueryServiceStatusEx is what hands back the pid — the CIM query the deep
/// probe uses would work too, but this keeps the whole launch on the SCM API.
DWORD startTrustedInstaller(QString *error)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        *error = win32Message(GetLastError());
        return 0;
    }

    SC_HANDLE service = OpenServiceW(scm, L"TrustedInstaller",
                                     SERVICE_START | SERVICE_QUERY_STATUS);
    if (!service) {
        *error = win32Message(GetLastError());
        CloseServiceHandle(scm);
        return 0;
    }

    DWORD pid = 0;
    SERVICE_STATUS_PROCESS status{};
    DWORD needed = 0;

    // Kick it, ignoring "already running". Then poll up to ~15s: the service can report
    // START_PENDING for a moment, and a pid only exists once it is actually RUNNING.
    if (!StartServiceW(service, 0, nullptr)) {
        const DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING)
            *error = win32Message(err);
    }

    for (int attempt = 0; attempt < 50; ++attempt) {
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                 reinterpret_cast<LPBYTE>(&status), sizeof(status), &needed)) {
            if (status.dwCurrentState == SERVICE_RUNNING && status.dwProcessId) {
                pid = status.dwProcessId;
                break;
            }
            // If it fell back to STOPPED between polls, ask again.
            if (status.dwCurrentState == SERVICE_STOPPED)
                StartServiceW(service, 0, nullptr);
        }
        Sleep(300);
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    if (!pid && error->isEmpty())
        *error = tr("ti.err.notRunning");
    return pid;
}

/// The command line to hand CreateProcess, and whether it opens a console window.
///
/// An executable is run directly. Anything else — a .bat, a .msi, a .ptx, a plain
/// document — is opened through `cmd /c start`, which resolves the file's association and
/// launches its handler as a child of the TrustedInstaller cmd, i.e. also as
/// TrustedInstaller. That is the one phrasing that covers "a program or a file" without
/// guessing what the file is.
QString buildCommandLine(const QString &program, const QString &arguments, bool *newConsole)
{
    const QString native = QDir::toNativeSeparators(program);
    const QString suffix = QFileInfo(program).suffix().toLower();
    const bool runnable = suffix == QLatin1String("exe") || suffix == QLatin1String("com")
                          || suffix == QLatin1String("scr");

    if (runnable) {
        // A console program gets its own console; a GUI one ignores the flag. Either way
        // the launched thing is what the user sees, so a fresh console is right here.
        *newConsole = true;
        QString cmd = QLatin1Char('"') + native + QLatin1Char('"');
        if (!arguments.isEmpty())
            cmd += QLatin1Char(' ') + arguments;
        return cmd;
    }

    // The wrapper cmd is plumbing, not something to show, so it runs windowless; the app
    // `start` opens appears on its own. The empty "" is start's title argument, which it
    // needs so a quoted path is not mistaken for one.
    *newConsole = false;
    const QString comspec = QString::fromLocal8Bit(qgetenv("ComSpec"));
    const QString shell = comspec.isEmpty() ? QStringLiteral("C:\\Windows\\System32\\cmd.exe")
                                            : comspec;
    QString cmd = QLatin1Char('"') + QDir::toNativeSeparators(shell)
                  + QStringLiteral("\" /c start \"\" \"") + native + QLatin1Char('"');
    if (!arguments.isEmpty())
        cmd += QLatin1Char(' ') + arguments;
    return cmd;
}

#endif   // Q_OS_WIN

} // namespace

bool available()
{
#ifdef Q_OS_WIN
    return Registry::isElevated();
#else
    return false;
#endif
}

QString resolve(const QString &program)
{
    const QString name = program.trimmed();
    if (name.isEmpty())
        return QString();
    if (QFileInfo::exists(name))
        return QDir::toNativeSeparators(QFileInfo(name).absoluteFilePath());
#ifdef Q_OS_WIN
    // Only a bare name is searched for; a path that does not exist is simply that.
    if (name.contains(QLatin1Char('\\')) || name.contains(QLatin1Char('/')))
        return QString();
    const bool hasExtension = !QFileInfo(name).suffix().isEmpty();

    // PATH first, as CreateProcess itself would search it.
    std::vector<wchar_t> buffer(32768);
    const std::wstring wname = name.toStdWString();
    const DWORD n = SearchPathW(nullptr, wname.c_str(), hasExtension ? nullptr : L".exe",
                                DWORD(buffer.size()), buffer.data(), nullptr);
    if (n > 0 && n < buffer.size())
        return QString::fromWCharArray(buffer.data(), int(n));

    // Then App Paths, per user and per machine, the value being the full path — often
    // quoted, sometimes with environment variables, which RegGetValue expands.
    const QString key = QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")
                        + name + (hasExtension ? QString() : QStringLiteral(".exe"));
    const std::wstring wkey = key.toStdWString();
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        DWORD size = 0;
        if (RegGetValueW(root, wkey.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, nullptr, &size)
                != ERROR_SUCCESS
            || size == 0)
            continue;
        std::vector<wchar_t> value(size / sizeof(wchar_t) + 1);
        if (RegGetValueW(root, wkey.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, value.data(), &size)
            != ERROR_SUCCESS)
            continue;
        QString path = QString::fromWCharArray(value.data()).trimmed();
        if (path.size() >= 2 && path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"')))
            path = path.mid(1, path.size() - 2);
        if (!path.isEmpty() && QFileInfo::exists(path))
            return QDir::toNativeSeparators(path);
    }
#endif
    return QString();
}

Result launch(const QString &program, const QString &arguments, const QString &workingDir)
{
    Result result;

#ifdef Q_OS_WIN
    if (program.trimmed().isEmpty()) {
        result.summary = tr("ti.err.noTarget");
        return result;
    }
    if (!Registry::isElevated()) {
        result.summary = tr("ti.err.notElevated");
        return result;
    }
    const QString target = resolve(program);
    if (target.isEmpty()) {
        result.summary = tr("ti.err.notFound");
        result.detail = QDir::toNativeSeparators(program);
        return result;
    }
    result.program = target;

    enablePrivilege(SE_DEBUG_NAME);

    QString error;
    const DWORD tiPid = startTrustedInstaller(&error);
    if (!tiPid) {
        result.summary = tr("ti.err.service");
        result.detail = error;
        return result;
    }

    // Only the right to be named as a parent; nothing is read out of the process.
    HANDLE tiProcess = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, tiPid);
    if (!tiProcess) {
        result.summary = tr("ti.err.open");
        result.detail = win32Message(GetLastError());
        return result;
    }

    // One attribute: the parent process. InitializeProcThreadAttributeList is called once
    // to size the buffer and once to fill it.
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    auto *attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attrSize));

    // Tracked apart from `prepared`, the way shell.cpp does it: a list that was
    // initialised has to go through DeleteProcThreadAttributeList before its buffer is
    // freed, and one that was not must not — freeing the buffer alone leaks what the
    // list allocated inside it.
    const bool initialised = attrList != nullptr
                             && InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize);
    const bool prepared = initialised
                          && UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
                                                       &tiProcess, sizeof(tiProcess), nullptr, nullptr);
    if (!prepared) {
        result.summary = tr("ti.err.prepare");
        result.detail = win32Message(GetLastError());
        if (initialised)
            DeleteProcThreadAttributeList(attrList);
        if (attrList)
            HeapFree(GetProcessHeap(), 0, attrList);
        CloseHandle(tiProcess);
        return result;
    }

    bool newConsole = true;
    const QString commandLine = buildCommandLine(target, arguments, &newConsole);

    // CreateProcess writes into the command-line buffer, so it cannot be a literal.
    std::wstring cmdBuffer = commandLine.toStdWString();
    cmdBuffer.push_back(L'\0');

    const QString effectiveDir = workingDir.isEmpty()
                                     ? QFileInfo(target).absolutePath()
                                     : workingDir;
    const std::wstring dirBuffer = QDir::toNativeSeparators(effectiveDir).toStdWString();

    // The interactive desktop, named explicitly. TrustedInstaller is a session-0 service,
    // and a child that inherits its token would otherwise land on the invisible session-0
    // desktop — the program would run and never appear. WinSta0\Default is the desk the
    // signed-in user is looking at. A mutable buffer because lpDesktop is not const.
    wchar_t desktop[] = L"WinSta0\\Default";

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.lpDesktop = desktop;
    startup.lpAttributeList = attrList;

    PROCESS_INFORMATION info{};
    const DWORD flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT
                        | (newConsole ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW);

    const BOOL created = CreateProcessW(
        nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, flags, nullptr,
        dirBuffer.empty() ? nullptr : dirBuffer.c_str(),
        reinterpret_cast<LPSTARTUPINFOW>(&startup), &info);

    if (created) {
        result.ok = true;
        result.pid = info.dwProcessId;
        result.summary = tr("ti.launched");
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
    } else {
        result.summary = tr("ti.err.create");
        result.detail = win32Message(GetLastError());
    }

    DeleteProcThreadAttributeList(attrList);
    HeapFree(GetProcessHeap(), 0, attrList);
    CloseHandle(tiProcess);
#else
    Q_UNUSED(program);
    Q_UNUSED(arguments);
    Q_UNUSED(workingDir);
    result.summary = tr("ti.err.windowsOnly");
#endif

    return result;
}

} // namespace TrustedInstaller
