// trustedinstaller.h — launch a program, or open a file, with the TrustedInstaller token.
//
// TrustedInstaller is the account Windows keeps for servicing itself. It owns the files
// and registry keys that even an administrator is refused until they first take ownership
// of them — which is what the Sahiplik feature does, one path at a time. Running a program
// under that account is the other way in: a shell, an editor, a registry tool that can
// already read and write all of it, without changing a single permission first.
//
// The technique is parent-process inheritance, the same one NSudo and PowerRun use. Start
// the TrustedInstaller service, then create the new process with the TrustedInstaller
// process named as its parent through a STARTUPINFOEX attribute list. A child inherits its
// parent's primary token, so it comes up as TrustedInstaller. Nothing is written anywhere
// on the machine to make this happen; the elevated token Arbitrium already runs with is
// what makes it allowed, and a standard token cannot do it at all.

#pragma once

#include <QString>

namespace TrustedInstaller {

struct Result
{
    bool ok = false;
    quint32 pid = 0;   ///< the launched process id, when ok
    QString summary;   ///< one translated line for the status bar and the row
    QString detail;    ///< the Win32 step that failed and its message, when not ok
};

/// Launches \a program under the TrustedInstaller account. An executable (.exe/.com/.scr)
/// is started directly; anything else — a script, a document, an installer — is opened
/// through the shell as a child of a TrustedInstaller cmd, so it runs in whatever handler
/// its type is associated with, also as TrustedInstaller. \a workingDir defaults to the
/// target's own folder. Windows only.
Result launch(const QString &program, const QString &arguments = QString(),
              const QString &workingDir = QString());

/// True when this can work at all: Windows, and an elevated token. The page dims its
/// controls and says why when it is false — a standard token cannot reach TrustedInstaller
/// however the launch is phrased.
bool available();

} // namespace TrustedInstaller
