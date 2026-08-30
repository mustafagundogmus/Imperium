// settingslinks.h — the curated list of Windows' own settings pages, applets and consoles
// behind the God Mode screen.
//
// This is the only file in the project whose data does not describe something to change:
// every entry is a place to *open*. The app writes nothing on its behalf — it hands the
// target to Windows and Windows draws its own dialog — which is why a link carries no
// registry entry, no script and no reversibility flag, only an id and a target.
//
// The target is one of four shapes, and the page decides what to do with each by looking
// at the string (see kindOf() in views/godmodepage.cpp):
//
//   "ms-settings:windowsupdate"   a URI, opened through QDesktopServices::openUrl, the
//                                 same call settingspage.cpp already uses for ms-settings:about
//   "shell:::{ED7BA470-…}"        a shell folder, handed to explorer.exe as an argument
//   "services.msc" / "ncpa.cpl"   a console document or a control-panel applet, opened by
//                                 mmc.exe / control.exe with the file's absolute path
//   "regedit.exe"                 a program, run from its absolute path
//
// Loaded out of the compiled resource (":/data/settings-links.json") rather than off disk,
// for the same reason catalog.json and actions.json are: Arbitrium is one portable file
// that people run out of Downloads and it always runs elevated, so a settings-links.json
// dropped next to the executable would otherwise be a stranger choosing what an
// administrator process launches.
//
// Labels are not in this file. An entry's name comes from the translation table as
// "godmode.<id>", and a group's heading as "godmode.group.<id>", so the ten languages are
// edited in one place instead of here — tools/check-data.py checks that every id has its
// key, because nothing at run time can (Locale::tr renders a missing key as itself).

#pragma once

#include <QString>
#include <QVector>

namespace SettingsLinks {

struct Link
{
    QString id;       ///< stable slug; the label is Locale::tr("godmode." + id)
    QString target;   ///< URI, shell: path, applet, console document or program
};

struct Group
{
    QString id;              ///< stable slug; the heading is Locale::tr("godmode.group." + id)
    QVector<Link> links;
};

/// Every group in the file, in file order. Parsed once on the first call.
const QVector<Group> &groups();

/// How many links there are across every group — the number the page subtitle reports.
int total();

} // namespace SettingsLinks
