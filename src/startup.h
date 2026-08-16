// startup.h — what this machine runs when you log in.
//
// Four places hold it: the Run keys under HKCU and HKLM, the 32-bit Run key, and the two
// Startup folders. None of those is where the on/off state lives, though — Task Manager
// does not delete a Run value when you disable an entry, it writes a twelve-byte blob to
//
//     …\Explorer\StartupApproved\Run | Run32 | StartupFolder
//
// whose first byte is 2 for enabled and 3 for disabled. That is the value this app writes
// too: it is reversible, it is what Windows itself reads, and an entry disabled here shows
// as disabled in Task Manager rather than as a mysteriously missing program.

#pragma once

#include <QString>
#include <QVector>

namespace Startup {

struct Entry
{
    QString name;       ///< the Run value's name, or the shortcut's file name
    QString command;    ///< what it runs
    QString source;     ///< "HKCU", "HKLM", "HKLM · 32 bit", "Başlangıç klasörü"
    bool enabled = true;

    // Where the on/off state is written.
    QString approvedHive;
    QString approvedPath;
    QString approvedValue;

    /// The blob currently sitting there, as the catalogue spells binary data — empty when
    /// the entry has never been toggled. Kept because Windows stamps the disable time
    /// into the tail of it, so the only string guaranteed to match what is there is the
    /// one that is already there.
    QString currentBlob;
};

/// Every startup entry on this machine. Read-only, no elevation.
QVector<Entry> enumerate();

/// The blob that means enabled / disabled, in the catalogue's comma-separated hex.
QString enabledBlob();
QString disabledBlob();

} // namespace Startup
