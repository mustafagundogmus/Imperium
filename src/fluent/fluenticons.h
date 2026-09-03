// fluenticons.h — the glyphs of the Fluent shell, and which row gets which.
//
// The handoff draws lucide-style line icons at three places: the rail (18px), the pane
// rows and the tweak rows' icon boxes (16px), all in the 24-unit grid at a 1.75 stroke.
// The prototype hand-traced a few; this file takes the real lucide drawings for the same
// meanings, reuses the ones icons.cpp already carries for the overview, and adds the rest.
//
// A tweak row's icon is chosen from what the row is about — a camera for the camera
// permission, a pin for location, a bell for notifications, an eye for telemetry — by
// looking at the row's own words, and falls back to its category's glyph. The prototype
// gives every row an icon of its own; the catalogue carries none, so this is how they are
// found rather than typed 539 times.

#pragma once

#include "../icons.h"

#include <QColor>
#include <QPixmap>
#include <QString>

struct Tweak;

namespace FluentIcons {

/// Renders \a g into a \a size box with a stroke of \a strokeUnits lucide units — 1.75 is
/// the handoff's weight, where Icons::lucide() fixes its own at exactly one pixel.
QPixmap draw(const Icons::Glyph &g, const QColor &c, int size, qreal strokeUnits, qreal dpr);

/// The glyph a page is listed with: a catalogue category by id, or one of the tool and
/// meta pages the sidebar knows (actions, cleaner, godmode, tilauncher, debloat, journal,
/// settings, about).
const Icons::Glyph &pageGlyph(const QString &id);

/// The glyph for one tweak row, by its words; \a categoryId decides the fallback.
const Icons::Glyph &tweakGlyph(const Tweak &t, const QString &categoryId);

namespace Lucide {

extern const Icons::Glyph House;
extern const Icons::Glyph Wrench;
extern const Icons::Glyph History;
extern const Icons::Glyph Settings;
extern const Icons::Glyph Bell;
extern const Icons::Glyph Megaphone;
extern const Icons::Glyph Search;
extern const Icons::Glyph MapPin;
extern const Icons::Glyph Camera;
extern const Icons::Glyph Mic;
extern const Icons::Glyph Folder;
extern const Icons::Glyph RefreshCw;
extern const Icons::Glyph Power;
extern const Icons::Glyph Keyboard;
extern const Icons::Glyph Mouse;
extern const Icons::Glyph Volume2;
extern const Icons::Glyph Bluetooth;
extern const Icons::Glyph Trash2;
extern const Icons::Glyph LayoutGrid;
extern const Icons::Glyph Sparkles;
extern const Icons::Glyph Cloud;
extern const Icons::Glyph Clipboard;
extern const Icons::Glyph Printer;
extern const Icons::Glyph Gamepad2;
extern const Icons::Glyph Rocket;
extern const Icons::Glyph Menu;
extern const Icons::Glyph Zap;
extern const Icons::Glyph KeyRound;
extern const Icons::Glyph Info;

} // namespace Lucide

} // namespace FluentIcons
