// icons.h — the mockup's hand-drawn 1px icons.
//
// Every glyph in the design is an inline SVG with a 12×12 (or 10×10 / 13×13) viewBox,
// `fill:none`, a 1px round-capped stroke and `stroke="currentColor"`. Rather than
// re-tracing them as QPainterPaths, the original path data is kept verbatim and handed
// to QSvgRenderer, so the rendered result is the same geometry the browser draws.
// Results are cached per (glyph, size, colour, dpr).
//
// A second set joined them later — lucide, for the Genel Bakış card titles. It is carried
// the same way and drawn by the same renderer; see the block at the bottom of this file.

#pragma once

#include <QColor>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace Icons {

/// Renders `<path d="..." fill="none" stroke="c" …>` from a viewBox of \a viewBox
/// units into a \a size-logical-pixel pixmap for the given device pixel ratio.
QPixmap strokePath(const QString &d, qreal viewBox, const QSize &size,
                   const QColor &c, qreal strokeWidth, qreal dpr);

/// Renders an arbitrary SVG fragment (used by the shapes that are not a single path).
QPixmap fragment(const QString &cacheKey, const QString &inner, qreal viewBox,
                 const QSize &size, qreal dpr);

// --- named glyphs from the design ------------------------------------------

QPixmap search(const QColor &c, qreal dpr);                 ///< 11×11 from a 12 viewBox
QPixmap windowMinimize(const QColor &c, qreal dpr);         ///< 10×10
QPixmap windowMaximize(const QColor &c, qreal dpr);         ///< 10×10
QPixmap windowRestore(const QColor &c, qreal dpr);          ///< 10×10
QPixmap windowClose(const QColor &c, qreal dpr);            ///< 10×10
QPixmap sort(const QColor &c, qreal dpr);                   ///< 13×13, stroke 1.1

/// Category glyph: 12×12, stroke 1, round cap and join.
QPixmap category(const QString &pathData, const QColor &c, qreal dpr);

// --- lucide ------------------------------------------------------------------
//
// A second set, drawn on by the Genel Bakış cards. Everything above was traced from the
// mockup, which drew eight glyphs; the overview page wants one per card and there are
// twenty-eight of them, which is twenty-eight chances to give the same corner two
// different radii by hand. lucide is already drawn the way this app draws — `fill:none`,
// a 1px-feeling round-capped stroke, `stroke="currentColor"` — which is the exact shape
// strokePath() was written to render, so the geometry is kept verbatim in lucide's own
// 24-unit grid instead of being re-traced into the 12-unit one, and goes through the same
// renderer and the same cache as everything else here.
//
// Fetched once at authoring time from https://api.iconify.design/lucide/<name>.svg and
// embedded in icons.cpp: this is a portable executable that has to draw itself offline,
// so nothing it paints is ever fetched at run time.
//
// lucide is ISC-licensed; the text is in resources/licenses/lucide-ISC.txt, carried into
// the release zip beside the font and Qt licences.

struct Glyph
{
    const char *name;     ///< lucide's own name, which doubles as the cache key
    const char *shapes;   ///< its elements verbatim, stripped of the stroke attributes
};

/// Renders \a g into a \a size-logical-pixel box in \a c.
///
/// The stroke is not lucide's own 2, nor any other constant: it is derived from \a size so
/// that it lands on exactly one logical pixel whatever box it is asked for — the hairline
/// weight everything else in this app is drawn in, and the same thing category() gets by
/// rendering a 12-unit viewBox into a 12px box. See lucideStroke() in icons.cpp.
QPixmap lucide(const Glyph &g, const QColor &c, int size, qreal dpr);

/// The twenty-eight the Genel Bakış cards use plus the one Hakkında adds, named as lucide
/// names them.
namespace Lucide {

extern const Glyph AppWindow;
extern const Glyph BatteryCharging;
extern const Glyph Box;
extern const Glyph CalendarClock;
extern const Glyph CircuitBoard;
extern const Glyph Clock;
extern const Glyph Cpu;
extern const Glyph Download;
extern const Glyph EyeOff;
extern const Glyph Gauge;
extern const Glyph Globe;
extern const Glyph HardDrive;
extern const Glyph HeartPulse;
extern const Glyph Layers;
extern const Glyph Lock;
extern const Glyph MemoryStick;
extern const Glyph Microchip;
extern const Glyph Monitor;
extern const Glyph Network;
extern const Glyph Package;
extern const Glyph Plug;
extern const Glyph ShieldCheck;
extern const Glyph SlidersHorizontal;
extern const Glyph SunMoon;
extern const Glyph Thermometer;
extern const Glyph TriangleAlert;
extern const Glyph User;
extern const Glyph Users;
extern const Glyph Wifi;

} // namespace Lucide

} // namespace Icons
