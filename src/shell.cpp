#include "shell.h"
#include "i18n.h"

#include "catalog.h"

#include <QRegularExpression>

#ifdef Q_OS_WIN
#  include <QCoreApplication>
#  include <QDir>
#  include <QEventLoop>
#  include <QFileInfo>

#  include <string>
#  include <vector>

#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>

#  include <sddl.h>
#  include <tlhelp32.h>
#endif

namespace Shell {
namespace {

#ifdef Q_OS_WIN

/// The message that asks the shell to shut itself down, posted to the taskbar window.
///
/// This is the same request the hidden "Exit Explorer" item makes — the one you reach with
/// Ctrl+Shift+right-click on the taskbar, which Aaron Margosis wrote up on MSDN in 2007 as
/// the way to stop the shell without killing it, put there for people who work on shell
/// extensions. Microsoft never documented the message itself: 0x5B4 is WM_USER+436, it is
/// private to Explorer's tray window, and every source for it is somebody who watched the
/// menu item send it. Windows 11 removed the menu item along with the taskbar's context
/// menu, so on that version there is not even a first-party gesture left to compare
/// against. The only thing this code does about that risk is refuse to trust it — nothing
/// below believes the shell stopped, or came back, because a message was posted. It looks,
/// and it terminates the process when looking says the message did nothing.
///
/// Politeness is not manners here. A killed shell loses whatever it had not yet written:
/// the desktop icon layout, the taskbar's own state, and — the reason act-icon-cache used
/// to kill it — the icon and thumbnail databases it keeps open and only closes properly on
/// the way out.
constexpr UINT ShellExitMessage = 0x5B4;

/// How long each stage of restartExplorer() is prepared to wait.
///
/// This runs on the GUI thread, from the apply overlay's button. The event loop is pumped
/// while it waits (see pumpUi below) so the window keeps painting, but the caller's stack
/// frame is held for the whole duration, so the numbers still have to be defensible.
///
/// The ordinary path is two or three seconds: a shell that is asked to exit is gone in
/// well under a second, no grace is owed after a clean exit (see restartExplorer), and the
/// new shell registers its window a second or two after it is started. The bad case is
/// where every stage runs to its limit — 2500 + 1500 + 1000 + 4000, and then up to three
/// launch attempts of which only the last can cost the full StartWaitMs — which is a bit
/// over twenty seconds. That is a long time, and it is spent deliberately: every one of
/// those stages only runs at full length when the user is sitting in front of an empty
/// desktop, and the alternative to waiting is telling them it failed while the shell they
/// are waiting for is still on its way up.
constexpr int PollStepMs = 100;
constexpr int ExitWaitMs = 2500;         ///< for the shell to act on the polite request
constexpr int ForcedExitWaitMs = 1500;   ///< for it to die after being terminated
constexpr int WindowClearMs = 1000;      ///< for its shell window registration to go
constexpr int RestartGraceMs = 4000;     ///< for Winlogon to put a shell back by itself
constexpr int StartWaitMs = 10000;       ///< for the one we started to take the desktop
constexpr int HandoffGraceMs = 2000;     ///< after the one we started exits by itself

/// How many parents restartExplorer() is willing to try. One per name in the candidate
/// list below, because a second process of the same name would fail for the same reason.
constexpr int MaxParents = 3;

/// Lets the interface repaint in the middle of a wait.
///
/// Without this the whole restart is a blocking call on the GUI thread and Windows marks
/// the window as not responding, which is exactly the wrong thing to do at the one moment
/// when Arbitrium is the only thing left on the screen: the taskbar and the desktop are
/// gone, and a frozen grey rectangle invites the user to kill the one program that is in
/// the middle of putting their shell back.
///
/// User input is excluded on purpose. This is called from inside a button's clicked
/// handler, and delivering a second click on that same button here would start a second
/// restart underneath the first one — one of them terminating the shell the other had just
/// started. Excluding input defers those clicks until after this returns instead, and the
/// re-entry guard in restartExplorer() covers anything that arrives by another route.
void pumpUi()
{
    if (QCoreApplication::instance())
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

/// True when a shell holds the desktop right now.
///
/// GetShellWindow() hands back the window Explorer registers when it takes the shell role
/// — Progman, owned by the same process that owns Shell_TrayWnd, which was confirmed by
/// reading both on this machine rather than assumed. It costs nothing, it cannot be fooled
/// by an explorer.exe that is running without being the shell, and it is null the instant
/// the shell is gone. It is the whole basis for this file being allowed to say "restarted".
bool shellIsUp()
{
    return GetShellWindow() != nullptr;
}

/// Waits up to \a budgetMs for a shell to appear, and answers whether one did.
///
/// \a child, when it is given, is the process this file started. Watching it turns the
/// worst case from "sit here for the full budget and then guess" into a fact: a shell that
/// exits on its own did not take the desktop and never will, and the caller can go and try
/// a different parent instead of spending the rest of the budget on a corpse. It is not
/// treated as an immediate failure, though, because there is one way a child that exits
/// still ends well — Explorer deciding it does not like its own token and handing the job
/// to the CreateExplorerShellUnelevatedTask scheduled task, which starts a shell this code
/// never sees. \a childExited is set so the caller can tell the two apart.
bool waitForShell(int budgetMs, HANDLE child = nullptr, bool *childExited = nullptr)
{
    if (childExited)
        *childExited = false;

    HANDLE watched = child;
    for (int remaining = budgetMs; remaining > 0; remaining -= PollStepMs) {
        if (shellIsUp())
            return true;
        if (watched && WaitForSingleObject(watched, 0) == WAIT_OBJECT_0) {
            if (childExited)
                *childExited = true;
            watched = nullptr;   // asked and answered; do not keep asking
            if (remaining > HandoffGraceMs)
                remaining = HandoffGraceMs;
        }
        pumpUi();
        Sleep(PollStepMs);
    }
    return shellIsUp();
}

/// Waits up to \a budgetMs for the shell window registration to go away.
bool waitForNoShell(int budgetMs)
{
    for (int waited = 0; waited < budgetMs; waited += PollStepMs) {
        if (!shellIsUp())
            return true;
        pumpUi();
        Sleep(PollStepMs);
    }
    return !shellIsUp();
}

/// The full path of explorer.exe, or an empty string when it is not where it should be.
///
/// Never the bare name. Explorer lives in the Windows directory rather than System32, so a
/// bare "explorer.exe" is resolved through PATH — an environment block this process
/// inherited from whoever started it, which an elevated process has no business trusting.
/// That is the same class of bug this repository already fixed for tbs.dll and
/// netapi32.dll. The Windows directory is asked of the API, which cannot be lied to the
/// way %SystemRoot% can.
QString explorerPath()
{
    wchar_t buffer[MAX_PATH] = {};
    const UINT written = GetWindowsDirectoryW(buffer, MAX_PATH);
    if (written == 0 || written >= MAX_PATH)
        return QString();

    QString path = QString::fromWCharArray(buffer, int(written));
    if (!path.endsWith(QLatin1Char('\\')))
        path += QLatin1Char('\\');
    path += QStringLiteral("explorer.exe");
    return QFileInfo::exists(path) ? path : QString();
}

/// The string form of the user a process is running as, for comparing two processes.
///
/// PROCESS_QUERY_LIMITED_INFORMATION is enough to open the token for TOKEN_QUERY, which is
/// worth stating because MSDN's page for OpenProcessToken still names the older and wider
/// PROCESS_QUERY_INFORMATION. It was measured here before this file relied on it: had it
/// been wrong, every candidate below would have been rejected and the user would have been
/// left with no shell and a message saying no parent could be found.
QString tokenUserSid(HANDLE process)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token))
        return QString();

    QString text;
    DWORD needed = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    if (needed > 0) {
        std::vector<BYTE> buffer(needed);
        if (GetTokenInformation(token, TokenUser, buffer.data(), needed, &needed)) {
            const auto *user = reinterpret_cast<const TOKEN_USER *>(buffer.data());
            LPWSTR sid = nullptr;
            if (ConvertSidToStringSidW(user->User.Sid, &sid)) {
                text = QString::fromWCharArray(sid);
                LocalFree(sid);
            }
        }
    }
    CloseHandle(token);
    return text;
}

/// True when \a process holds the plain interactive token the shell is supposed to have:
/// not elevated, and Medium integrity.
///
/// Both halves are checked because the second does not follow from the first. Measured on
/// the machine this was written on: ctfmon.exe runs at High integrity (0x3000) with
/// TokenIsElevated reporting 0, so an elevation test on its own would have accepted it and
/// handed the new shell a High-integrity token — a quieter version of the bug being fixed
/// here. Low and AppContainer processes are excluded from the other side by the same
/// comparison, which matters because SearchHost.exe sits in the session at 0x1000.
bool hasPlainUserToken(HANDLE process)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token))
        return false;

    bool plain = false;
    TOKEN_ELEVATION elevation{};
    DWORD needed = 0;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &needed)
        && elevation.TokenIsElevated == 0) {
        DWORD size = 0;
        GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
        if (size > 0) {
            std::vector<BYTE> buffer(size);
            if (GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), size, &size)) {
                const auto *label = reinterpret_cast<const TOKEN_MANDATORY_LABEL *>(buffer.data());
                // The level is the last sub-authority. Counted rather than assumed to be
                // the first: a SID with none at all would otherwise be read at index -1.
                const UCHAR count = *GetSidSubAuthorityCount(label->Label.Sid);
                if (count > 0) {
                    const DWORD rid = *GetSidSubAuthority(label->Label.Sid, count - 1);
                    plain = rid >= SECURITY_MANDATORY_MEDIUM_RID
                            && rid < SECURITY_MANDATORY_HIGH_RID;
                }
            }
        }
    }
    CloseHandle(token);
    return plain;
}

/// Fills \a out with open handles to the processes in this session whose token is the one
/// the shell should come back with, best first, and returns how many there are. The caller
/// owns and closes them.
///
/// Handles rather than process ids, and opened here rather than looked up again later, so
/// that there is no window in which the process can exit and Windows can hand its number
/// to something else. A handle keeps the process object alive, so the thing checked below
/// is the same thing CreateProcessW is later given. The token is examined through the very
/// handle that will be used, for the same reason.
///
/// The list is short and it is named on purpose. A new process inherits its parent's job
/// object along with its token, and on Windows 10 and 11 practically everything in an
/// interactive session is already in one — measured here, explorer.exe itself included —
/// so "is it in a job" cannot be used to sort the safe parents from the dangerous ones.
/// What can be used is knowing whose job it is: sihost.exe and taskhostw.exe are started
/// for the session by the User Manager and the Task Scheduler, not by the shell, so they
/// are still there when the shell is not, and their jobs are the session's own rather than
/// an application sandbox. A browser tab or a packaged app would satisfy every token test
/// in this file and could still hand the shell a job that kills it, so no general sweep of
/// "any medium-integrity process" is done: a wrong parent is worse than no parent, because
/// no parent is a message the user can act on. StartMenuExperienceHost.exe and the
/// RuntimeBrokers pass every test here and are left out for exactly that reason.
///
/// \a ownerSid, when it is known, is the user the outgoing shell belonged to. It is
/// compared rather than the current process's own user because those are not always the
/// same person: an administrator who elevated Arbitrium over somebody else's shoulder is
/// not who the desktop belongs to, and the shell must come back for whoever is sitting
/// there.
int collectUnelevatedParents(const QString &ownerSid, HANDLE *out, int max)
{
    // explorer.exe first, on the chance that one survived — a folder window running in a
    // separate process is the outgoing shell's own token and its own job, exactly what we
    // want back. sihost and taskhostw are the fallbacks that outlive the shell.
    static const wchar_t *const candidates[] = {L"explorer.exe", L"sihost.exe",
                                                L"taskhostw.exe"};

    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session))
        return 0;

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    int count = 0;
    for (const wchar_t *const name : candidates) {
        if (count >= max)
            break;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        // Process32FirstW restarts the walk over the same snapshot, so the outer loop is
        // what keeps the preference order above meaningful rather than taking whatever
        // comes first.
        if (!Process32FirstW(snapshot, &entry))
            break;
        do {
            if (lstrcmpiW(entry.szExeFile, name) != 0)
                continue;

            DWORD candidateSession = 0;
            if (!ProcessIdToSessionId(entry.th32ProcessID, &candidateSession)
                || candidateSession != session)
                continue;

            // PROCESS_CREATE_PROCESS is the right to be named as a parent;
            // PROCESS_QUERY_LIMITED_INFORMATION is what the two token checks need.
            const HANDLE process = OpenProcess(PROCESS_CREATE_PROCESS
                                                   | PROCESS_QUERY_LIMITED_INFORMATION,
                                               FALSE, entry.th32ProcessID);
            if (!process)
                continue;

            if (hasPlainUserToken(process)
                && (ownerSid.isEmpty() || tokenUserSid(process) == ownerSid)) {
                out[count++] = process;
                break;   // one per name; a second copy would fail for the same reasons
            }
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return count;
}

/// Whether Winlogon will put a shell back by itself when one dies unexpectedly.
///
/// Read rather than assumed, because the answer decides whether this file waits at all.
/// AutoRestartShell is the switch behind that behaviour and its default is on, so an
/// unreadable or absent value is taken as on — the cost of being wrong that way is a few
/// seconds of waiting, and the cost of being wrong the other way is racing Winlogon and
/// leaving the user with a stray folder window.
bool autoRestartShellEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon", 0,
                      KEY_QUERY_VALUE, &key)
        != ERROR_SUCCESS)
        return true;

    DWORD value = 1;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS status = RegQueryValueExW(key, L"AutoRestartShell", nullptr, &type,
                                            reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(value))
        return true;
    return value != 0;
}

/// How the shell went, which decides what happens next.
enum class Stop {
    NothingToStop,   ///< there was no shell when we looked
    Exited,          ///< it took the request and left on its own
    Terminated,      ///< it ignored the request and had to be killed
    Failed           ///< it is still there
};

/// Ends the running shell and reports how. \a ownerSid is filled with the user it was
/// running as while it is still alive to be asked.
///
/// The polite request goes first and the kill is the fallback, not the other way round.
/// taskkill /F was worse than it looked: it force-terminated the shell before it could
/// save anything, and `/IM explorer.exe` from an elevated token matches by name across the
/// whole machine — on a computer with a second user signed in and switched away, it took
/// their shell down too. What goes now is one process: the one that owns the shell window.
Stop stopShell(QString *ownerSid)
{
    const HWND shell = GetShellWindow();
    if (!shell)
        return Stop::NothingToStop;   // the user may already be looking at bare wallpaper

    DWORD pid = 0;
    GetWindowThreadProcessId(shell, &pid);

    // SYNCHRONIZE so the exit can be waited on rather than polled for, which is the
    // difference between knowing the process is gone and knowing its window is.
    const HANDLE process = pid ? OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE
                                                 | PROCESS_QUERY_LIMITED_INFORMATION,
                                             FALSE, pid)
                               : nullptr;
    if (process && ownerSid)
        *ownerSid = tokenUserSid(process);

    bool asked = false;
    const HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    DWORD trayPid = 0;
    if (tray)
        GetWindowThreadProcessId(tray, &trayPid);

    // Only ever posted to a window that has been shown to belong to the shell process.
    // Finding a Shell_TrayWnd is not the same as finding the shell's own: an undocumented
    // message aimed at a stranger's window is an unbounded thing to do, and a message
    // posted to a window nobody verified would then be waited on and reported as a stop.
    if (tray && pid && trayPid == pid) {
        // High integrity posting to a Medium-integrity window: UIPI blocks the other
        // direction, not this one, so an elevated Arbitrium is allowed to ask.
        PostMessageW(tray, ShellExitMessage, 0, 0);
        asked = true;
    }

    bool exited = false;
    if (asked) {
        exited = process ? WaitForSingleObject(process, ExitWaitMs) == WAIT_OBJECT_0
                         : waitForNoShell(ExitWaitMs);
    }

    // The result of this wait is deliberately not the verdict. It is here to give the
    // kill time to land; whether the desktop is actually free is decided below, by
    // looking at the desktop.
    if (!exited && process) {
        TerminateProcess(process, 1);
        WaitForSingleObject(process, ForcedExitWaitMs);
    }
    if (process)
        CloseHandle(process);

    // The process object being signalled is not the same thing as the desktop being free.
    // The shell window registration is dropped as the window is destroyed during rundown,
    // and everything after this point is a judgement about a *new* shell — so the old
    // one's window has to be confirmed gone first. Without this the grace wait below can
    // see the corpse on its very first poll, call it a restart, and return true over an
    // empty desktop, which is the exact failure this whole function exists to prevent.
    if (!waitForNoShell(WindowClearMs))
        return Stop::Failed;

    if (exited)
        return Stop::Exited;
    // It was killed, or it went for a reason nothing here accounted for. Both are reported
    // the same way, because what the caller does with this is decide whether to give
    // Winlogon a moment, and Winlogon's own trigger is a shell that died without asking.
    return Stop::Terminated;
}

/// Starts \a program with \a parent named as its parent process, so it runs with that
/// process's token instead of ours. On success \a child holds a handle to it, which the
/// caller closes; on failure \a win32Error says why.
///
/// The technique is the one trustedinstaller.cpp already uses and documents — a
/// STARTUPINFOEX attribute list carrying PROC_THREAD_ATTRIBUTE_PARENT_PROCESS — pointed
/// the other way. There it reaches up, to a SYSTEM service, to gain rights; here it reaches
/// down, to an ordinary process in the user's session, to give them up. MSDN's list of what
/// a child takes from the process named this way says it in as many words: handles, the
/// device map, affinity, priority, quota, the job object, "and the process token".
///
/// It is written out here rather than shared with that file because the two calls have
/// nothing else in common: launch() has to start a service first, and routes anything that
/// is not an executable through cmd's `start`, neither of which means anything for the
/// shell. Exporting it from TrustedInstaller would have put the shell's restart inside a
/// module whose header opens by explaining that it is about servicing accounts.
///
/// One thing this does not do is rebuild the environment. The child inherits Arbitrium's
/// block, and the shell's environment is inherited in turn by everything the user starts
/// for the rest of the session. Doing it properly means CreateEnvironmentBlock against the
/// parent's token, which is in userenv, which is not linked. In the ordinary case — an
/// administrator elevating their own session — the two blocks describe the same profile
/// and it does not matter; where it would matter is a shell restarted for a different user
/// than the one Arbitrium is running as, and that is a known and deliberate gap.
bool startShellWithParent(const QString &program, HANDLE parent, HANDLE *child,
                          DWORD *win32Error)
{
    *child = nullptr;
    *win32Error = 0;

    // One attribute. InitializeProcThreadAttributeList is called once to size the buffer
    // and once to fill it.
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    auto *attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attrSize));
    if (!attrList) {
        *win32Error = DWORD(ERROR_OUTOFMEMORY);
        return false;
    }

    // Tracked separately from `prepared` so the failure path can tell whether the list was
    // ever initialised. DeleteProcThreadAttributeList must be called on one that was, and
    // must not be called on one that was not, and freeing the buffer is not a substitute:
    // the list owns state inside it that Delete is what releases.
    const bool initialised = InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize);
    // Not const, because UpdateProcThreadAttribute takes the address of the handle.
    HANDLE parentForAttribute = parent;
    const bool prepared = initialised
                          && UpdateProcThreadAttribute(attrList, 0,
                                                       PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
                                                       &parentForAttribute,
                                                       sizeof(parentForAttribute), nullptr,
                                                       nullptr);
    if (!prepared) {
        *win32Error = GetLastError();
        if (initialised)
            DeleteProcThreadAttributeList(attrList);
        HeapFree(GetProcessHeap(), 0, attrList);
        return false;
    }

    const std::wstring image = QDir::toNativeSeparators(program).toStdWString();
    std::wstring command = L'"' + image + L'"';
    command.push_back(L'\0');   // CreateProcess writes into this buffer

    // No desktop is named, unlike the TrustedInstaller path: that one has to say
    // WinSta0\Default because its parent is a session-0 service and the child would land
    // on the invisible desktop. This parent is a process in our own session, on the same
    // desk we are on, so inheriting is already right.
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attrList;

    // The Windows directory, and not the one this process happens to be in. Arbitrium is a
    // portable executable that is often run from a USB stick or a downloads folder, and a
    // current directory is a directory handle the shell would then hold open for the rest
    // of the session — the user could not eject the stick they started it from.
    const std::wstring workingDir = QDir::toNativeSeparators(QFileInfo(program).absolutePath())
                                        .toStdWString();

    PROCESS_INFORMATION info{};
    const BOOL created = CreateProcessW(image.c_str(), command.data(), nullptr, nullptr, FALSE,
                                        EXTENDED_STARTUPINFO_PRESENT, nullptr,
                                        workingDir.c_str(),
                                        reinterpret_cast<LPSTARTUPINFOW>(&startup), &info);
    if (created) {
        CloseHandle(info.hThread);
        *child = info.hProcess;   // kept, so the wait can tell "still starting" from "gone"
    } else {
        *win32Error = GetLastError();
    }

    DeleteProcThreadAttributeList(attrList);
    HeapFree(GetProcessHeap(), 0, attrList);
    return created != FALSE;
}

/// Fills \a error with what went wrong, and adds how to get a desktop back by hand — but
/// only when there is really no shell.
///
/// The distinction is the point. "Explorer would not stop" leaves the user with the taskbar
/// they already had and no reason to open Task Manager; every other failure here leaves
/// them with nothing, and a person with no shell needs the four steps, not a diagnosis.
void fail(QString *error, const QString &cause)
{
    if (!error)
        return;
    *error = cause;
    if (!shellIsUp())
        *error += QStringLiteral(" · ") + Locale::tr(QStringLiteral("err.explorerRecover"));
}

/// True while a restart is already running on this thread.
///
/// pumpUi() excludes user input, so the button cannot deliver a second click into the
/// middle of one. This guard is for anything that arrives by another route — a timer, a
/// queued call — because two of these running at once is the one combination that can
/// leave the machine worse off than doing nothing: the second one terminates the shell the
/// first one has just started, and then both of them wait for a shell that nobody is
/// starting any more.
bool restartInProgress = false;

/// Holds that flag for as long as it is in scope.
///
/// RAII rather than an assignment before every return, because pumpUi() runs other
/// people's slots and a throw out of one of them would otherwise leave the flag set for
/// the life of the process. With it stuck on, every later press of the restart button
/// would take the branch above and decline to do anything at all — a bug that only shows
/// up after something else has already gone wrong, which is the worst kind to have here.
struct RestartGuard
{
    RestartGuard() { restartInProgress = true; }
    ~RestartGuard() { restartInProgress = false; }
    RestartGuard(const RestartGuard &) = delete;
    RestartGuard &operator=(const RestartGuard &) = delete;
};

#endif   // Q_OS_WIN

} // namespace

bool needsExplorerRestart(const Tweak &tweak)
{
    // Matched against the key path rather than stored per tweak: the catalogue is
    // generated from the tutorials, and this rule stays correct as it grows.
    static const QRegularExpression shellKeys(
        QStringLiteral("CurrentVersion\\\\(Explorer|Policies\\\\Explorer)"
                       "|\\\\Explorer\\\\Advanced"
                       "|ContentDeliveryManager"
                       "|Windows\\\\Shell"
                       "|\\\\DWM\\b"
                       "|Control Panel\\\\Desktop"
                       "|Themes\\\\Personalize"
                       "|\\\\Feeds\\b"
                       "|\\\\Search\\b"
                       "|CLSID"
                       "|\\\\Dsh\\b"),
        QRegularExpression::CaseInsensitiveOption);

    for (const RegistryEntry &entry : tweak.reg)
        if (shellKeys.match(entry.path).hasMatch())
            return true;
    return false;
}

bool restartExplorer(QString *error)
{
#ifdef Q_OS_WIN
    if (restartInProgress) {
        // The outer call is still working. Say what is actually true rather than starting
        // a second restart: a shell either is on the desktop or is not.
        if (shellIsUp())
            return true;
        fail(error, Locale::tr(QStringLiteral("err.explorerNotBack")));
        return false;
    }
    const RestartGuard guard;

    // Resolved before anything is stopped. A machine that cannot produce this path is one
    // where the shell must not be taken down, because nothing here could put it back.
    const QString explorer = explorerPath();
    if (explorer.isEmpty()) {
        fail(error, Locale::tr(QStringLiteral("err.explorerMissing")));
        return false;
    }

    QString ownerSid;
    const Stop stopped = stopShell(&ownerSid);
    if (stopped == Stop::Failed) {
        fail(error, Locale::tr(QStringLiteral("err.explorerStop")));
        return false;
    }

    // Winlogon gets its moment, but only in the case where it is going to take it.
    //
    // AutoRestartShell is what puts the shell back when it dies unexpectedly, and when it
    // does the job it does it better than this file can — the same way logon does, with
    // the session's real token. But it is only owed a moment after a kill. A shell that
    // was asked to exit and did is not a crash, and the whole reason that hidden menu item
    // leaves you staring at the wallpaper is that Windows takes the request at its word
    // (Aaron Margosis, "How to cleanly stop Explorer.exe"). Waiting after a clean exit
    // would be four seconds of nothing on every single ordinary run; worse, waiting too
    // *little* after a kill means starting a shell while Winlogon is starting one too, and
    // the loser of that race turns into a folder window the user did not ask for. So the
    // wait happens exactly when Windows has said it will act, and is long enough to matter
    // when it does.
    if (stopped == Stop::Terminated && autoRestartShellEnabled()
        && waitForShell(RestartGraceMs)) {
        return true;
    }

    // Nothing came back, so it has to be started here — and it is this step, not the kill,
    // that the old code got wrong. A process started by an elevated process inherits the
    // elevated token, and Explorer will not be the interactive shell with one: it hands the
    // job to a scheduled task that starts it again unelevated, and exits (Raymond Chen,
    // "What is the CreateExplorerShellUnelevatedTask scheduled task?", 2022). That task is
    // created on demand, it can be absent, disabled or pruned by exactly the sort of
    // debloating this app does, and nothing about it is guaranteed — which is why the shell
    // came back on some machines and not on others. Naming an unelevated parent means the
    // new shell has the right token from its first instruction and needs no trampoline.
    HANDLE parents[MaxParents] = {};
    const int parentCount = collectUnelevatedParents(ownerSid, parents, MaxParents);
    if (parentCount == 0) {
        fail(error, Locale::tr(QStringLiteral("err.explorerNoParent")));
        return false;
    }

    bool up = false;
    bool everStarted = false;
    DWORD lastError = 0;
    for (int i = 0; i < parentCount && !up; ++i) {
        // Between the grace wait and here, a snapshot of every process on the machine was
        // walked. That is not free, and a shell that arrived during it is still a shell.
        if (shellIsUp()) {
            up = true;
            break;
        }

        HANDLE child = nullptr;
        DWORD win32Error = 0;
        if (!startShellWithParent(explorer, parents[i], &child, &win32Error)) {
            lastError = win32Error;
            continue;   // that parent is no good; there may be another
        }
        everStarted = true;

        // Started is not restarted. The process exists either way; only GetShellWindow can
        // say whether it took the desktop, and until it does this function has nothing to
        // report.
        bool childExited = false;
        up = waitForShell(StartWaitMs, child, &childExited);
        CloseHandle(child);

        // A child that is still alive and has simply not taken the desktop is not helped
        // by starting a second one alongside it — that is how a user ends up with two
        // Explorers. Only a child that exited on its own is worth answering with a
        // different parent, and that is the case this loop exists for: the residual risk
        // of naming a parent is its job object, and sihost's job is not taskhostw's.
        if (!up && !childExited)
            break;
    }

    for (int i = 0; i < parentCount; ++i)
        CloseHandle(parents[i]);

    if (up)
        return true;
    if (!everStarted) {
        fail(error, Locale::tr(QStringLiteral("err.explorerStart"))
                        + QStringLiteral(" (0x%1)").arg(lastError, 8, 16, QLatin1Char('0')));
        return false;
    }
    fail(error, Locale::tr(QStringLiteral("err.explorerNotBack")));
    return false;
#else
    if (error)
        *error = Locale::tr(QStringLiteral("err.windowsOnly"));
    return false;
#endif
}

} // namespace Shell
