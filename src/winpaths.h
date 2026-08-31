// winpaths.h — the Windows directories, asked of the API rather than of the environment.
//
// Arbitrium always runs elevated. That single fact is what this file is for: an elevated
// process inherits its environment block and its PATH from whoever started it, so every
// bare program name is a decision to run whatever that person's PATH resolves it to,
// inside an administrator token. The project has fixed this before — mmc.exe and
// control.exe on the God Mode page, tbs.dll and netapi32.dll in sysinfo.cpp — but three
// PowerShell launches were missed, and they are the ones on the startup path.
//
// There is a second reason, and it is what sent me looking: QStandardPaths::findExecutable
// does not merely trust PATH, it *walks* it, stat-ing a candidate in every entry until one
// answers. A PATH holding a mapped drive that is no longer reachable therefore blocks for
// the SMB timeout — tens of seconds — on the UI thread, at startup, before the window can
// paint. It costs nothing on a machine whose PATH is all local, which is why this is the
// kind of fault that reaches users and never the developer.
//
// GetSystemDirectoryW and GetWindowsDirectoryW cannot be lied to that way. The literal
// fallbacks are for a machine already too broken to launch anything.

#pragma once

#include <QString>

namespace WinPaths {

/// %SystemRoot%\System32, from GetSystemDirectoryW. Never from $SystemRoot.
QString system32();

/// %SystemRoot% itself, from GetWindowsDirectoryW. Not the same directory: regedit.exe
/// and explorer.exe live here, and regedit.exe has never been in System32.
QString windows();

/// The absolute path to Windows PowerShell 5.1, which is a fixed location on every
/// supported build — System32\WindowsPowerShell\v1.0\powershell.exe — and is the only
/// PowerShell this application asks for. It deliberately does not look for pwsh.exe:
/// PowerShell 7 is a separate, user-installed product whose presence and location are not
/// something an elevated process should discover from PATH.
///
/// Returns an empty string when the file is not there, so a caller can report the machine
/// rather than launch something else that happens to answer to the same name.
QString powershell();

} // namespace WinPaths
