// css.h — the slice of the CSS box model this design actually relies on.
//
// The handoff is expressed in CSS, and two of its behaviours have no direct Qt
// equivalent: line boxes with half-leading (`line-height:1.45`) and baseline
// alignment across differently sized fonts (`align-items:baseline`). Getting those
// wrong shifts text by 1–3px everywhere, so they are modelled explicitly here and
// every widget draws through these helpers rather than through Qt's own centring.

#pragma once

#include <QFont>
#include <QRectF>
#include <QString>

class QPainter;
class QColor;

namespace Css {

/// CSS `line-height: normal` — for IBM Plex this is ascent + descent (1.3em).
qreal normalLine(const QFont &f);

/// CSS `line-height: <factor>` — a multiple of the font size.
qreal line(const QFont &f, qreal factor);

qreal ascent(const QFont &f);
qreal descent(const QFont &f);
qreal textWidth(const QFont &f, const QString &s);

/// Baseline of a line box of height \a lineH whose top edge sits at \a lineTop.
/// Half-leading is split evenly above and below, exactly as CSS does.
qreal baseline(const QFont &f, qreal lineTop, qreal lineH);

/// Baseline of a single line box vertically centred in \a box (`align-items:center`).
qreal centeredBaseline(const QFont &f, const QRectF &box, qreal lineH = -1.0);

/// Draws one line of text sitting on \a baselineY. \a box supplies the horizontal
/// bounds; \a align may be AlignLeft / AlignRight / AlignHCenter. When \a elide is
/// true the text is truncated with an ellipsis to fit \a box (CSS text-overflow).
void drawText(QPainter *p, const QRectF &box, qreal baselineY, const QFont &f,
              const QColor &c, const QString &text,
              Qt::Alignment align = Qt::AlignLeft, bool elide = false);

/// Convenience: draw a vertically centred single line inside \a box.
void drawCentered(QPainter *p, const QRectF &box, const QFont &f, const QColor &c,
                  const QString &text, Qt::Alignment align = Qt::AlignLeft,
                  bool elide = false);

/// Turkish-correct `text-transform: uppercase` (i → İ, ı → I).
QString upperTr(const QString &s);

/// Fills a 1px hairline. Kept separate so the 1px stays 1 *logical* px on hi-dpi.
void hairline(QPainter *p, const QRectF &r, const QColor &c);

} // namespace Css
