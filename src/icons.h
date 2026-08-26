// icons.h — the mockup's hand-drawn 1px icons.
//
// Every glyph in the design is an inline SVG with a 12×12 (or 10×10 / 13×13) viewBox,
// `fill:none`, a 1px round-capped stroke and `stroke="currentColor"`. Rather than
// re-tracing them as QPainterPaths, the original path data is kept verbatim and handed
// to QSvgRenderer, so the rendered result is the same geometry the browser draws.
// Results are cached per (glyph, size, colour, dpr).

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

} // namespace Icons
