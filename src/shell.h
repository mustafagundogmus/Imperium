// shell.h — the one place that talks to explorer.exe.
//
// A great many tweaks only take effect once the shell re-reads the registry, which in
// practice means restarting Explorer. That closes every open Explorer window, so it is
// never done automatically: the app offers it after an apply and the user decides.
//
// It is also the one place, now. The three actions in actions.json that used to stop the
// shell themselves no longer do, because a script running under Arbitrium's elevated
// PowerShell cannot put it back the way it found it — see restartExplorer() below for what
// that costs and why. An action that needs the shell restarted says so in its result line
// and leaves the restart to the button on the apply overlay, which comes here.

#pragma once

#include <QString>

struct Tweak;

namespace Shell {

/// True when \a tweak writes somewhere the shell only reads at startup — Explorer's own
/// settings, the taskbar, DWM, the desktop or the content-delivery keys.
bool needsExplorerRestart(const Tweak &tweak);

/// Restarts the interactive shell, and answers whether there is one.
///
/// Three things have to be true at the end, and the function is written around checking
/// them rather than around performing steps:
///
///   * The shell stops. It is asked to exit first, the way the hidden "Exit Explorer" menu
///     item asks, so that it saves its state and lets go of the files it holds open; it is
///     only terminated if it does not go. Either way the old shell window is confirmed
///     gone before anything that follows is believed, because a process that has exited
///     and a desktop that is free are not the same fact.
///   * The shell comes back **unelevated**. Arbitrium always runs as administrator, and a
///     process it starts inherits that token. Explorer will not be the interactive shell
///     with an elevated one — it bounces the job to a scheduled task that starts it again
///     as the user, and that task can be missing on a machine somebody has been debloating.
///     So the new shell is created with an ordinary process in the user's session named as
///     its parent, and inherits that process's token instead of ours. Up to three such
///     parents are tried, because the one risk of naming a parent is the job object that
///     comes with the token, and the candidates do not share a job.
///   * The shell is verified. GetShellWindow() is polled for a bounded time, and true is
///     returned only when a shell actually holds the desktop. A restart Windows performed
///     on its own counts, because the test is the desktop, not who put it there.
///
/// On false, \a error carries the stage that failed and — whenever the user has genuinely
/// been left without a desktop — how to get one back by hand from Task Manager. Every path
/// out of here is one of those two answers; there is no branch that reports success it has
/// not looked at, and no branch that returns true without GetShellWindow() having answered.
///
/// Blocks the calling thread. Two or three seconds in the ordinary case; a little over
/// twenty in the worst one, where every wait runs to its limit and every candidate parent
/// is tried. The Qt event loop is pumped throughout so the window keeps painting — user
/// input is excluded while it does, so a second press of the button that called this
/// arrives after it returns rather than inside it.
bool restartExplorer(QString *error = nullptr);

} // namespace Shell
