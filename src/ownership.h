// ownership.h — taking a file or folder away from TrustedInstaller, and giving it back.
//
// This exists because a shell verb cannot elevate itself. Windows elevates exactly one
// verb name, `runas`, and a key can only hold it once — which leaves no way to offer
// both directions from the same menu. So the menu entries call Arbitrium instead, and
// Arbitrium is manifested `requireAdministrator`: launching it raises the UAC prompt and
// the work happens with a token that can actually do it.
//
// The heavy lifting is left to takeown and icacls rather than reimplemented against the
// security APIs: they are the tools that are known to cope with the awkward cases
// (reparse points, deny ACEs, a folder whose children disagree with it), and being wrong
// about a system file's ACL is not a cheap mistake.

#pragma once

#include <QString>

namespace Ownership {

struct Result
{
    bool ok = false;
    QString summary;   ///< one line, for the dialog's title area
    QString detail;    ///< what the tools printed, for the expandable part
};

/// Makes the Administrators group the owner of \a path and grants it full control.
/// Folders are done recursively.
Result take(const QString &path);

/// Puts \a path back under NT SERVICE\TrustedInstaller. Worth doing to a system file
/// once you are finished with it: while it is owned by Administrators, Windows Update
/// cannot service it.
Result giveBack(const QString &path);

} // namespace Ownership
