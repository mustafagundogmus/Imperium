// shell.h — the one place that talks to explorer.exe.
//
// A great many tweaks only take effect once the shell re-reads the registry, which in
// practice means restarting Explorer. That closes every open Explorer window, so it is
// never done automatically: the app offers it after an apply and the user decides.

#pragma once

#include <QString>

struct Tweak;

namespace Shell {

/// True when \a tweak writes somewhere the shell only reads at startup — Explorer's own
/// settings, the taskbar, DWM, the desktop or the content-delivery keys.
bool needsExplorerRestart(const Tweak &tweak);

/// Ends explorer.exe and starts it again. Returns false if the process could not be
/// stopped; a restart that Windows performs on its own still counts as success.
bool restartExplorer(QString *error = nullptr);

} // namespace Shell
