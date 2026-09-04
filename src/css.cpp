#include "css.h"
#include "i18n.h"
#include "theme.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QFontMetricsF>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>

namespace Css {

qreal normalLine(const QFont &f)
{
    return QFontMetricsF(f).height();
}

qreal line(const QFont &f, qreal factor)
{
    return Theme::pixelSize(f) * factor;
}

qreal ascent(const QFont &f)
{
    return QFontMetricsF(f).ascent();
}

qreal descent(const QFont &f)
{
    return QFontMetricsF(f).descent();
}

qreal textWidth(const QFont &f, const QString &s)
{
    return QFontMetricsF(f).horizontalAdvance(s);
}

qreal baseline(const QFont &f, qreal lineTop, qreal lineH)
{
    const QFontMetricsF fm(f);
    const qreal halfLeading = (lineH - (fm.ascent() + fm.descent())) / 2.0;
    return lineTop + halfLeading + fm.ascent();
}

qreal centeredBaseline(const QFont &f, const QRectF &box, qreal lineH)
{
    const qreal h = lineH > 0 ? lineH : normalLine(f);
    return baseline(f, box.center().y() - h / 2.0, h);
}

void drawText(QPainter *p, const QRectF &box, qreal baselineY, const QFont &f,
              const QColor &c, const QString &text, Qt::Alignment align, bool elide)
{
    if (text.isEmpty())
        return;

    p->save();
    p->setFont(f);
    p->setPen(c);

    // Arabic is written right to left, and Qt infers a string's direction from its first
    // strong character. That is right for a sentence and wrong for the many rows here that
    // begin with a neutral — a digit, a bracket, a "·" — where the leading punctuation ends
    // up on the wrong side of the words. Marking the paragraph direction explicitly puts
    // the bidi algorithm in the right frame for the whole run.
    //
    // Only the direction. Mirroring the layout as well is a separate, much larger job:
    // essentially every widget here paints its own geometry, and flipping the text
    // alignment without flipping the boxes around it would move the labels away from the
    // values they belong to.
    const bool rtl = Locale::isRtl();

    QString s = text;
    const QFontMetricsF fm(f);
    qreal w = fm.horizontalAdvance(s);
    if (elide && w > box.width()) {
        // ElideRight for both: QFontMetrics elides the logical string, and the bidi
        // reorder at paint time places the ellipsis on the reading-end side on its own
        // — so an RTL label keeps its start and drops its tail, the same as an LTR one.
        // Deriving the mode from the language double-flipped it and cut Arabic labels
        // off at the front.
        s = fm.elidedText(s, Qt::ElideRight, box.width());
        w = fm.horizontalAdvance(s);
    }

    qreal x = box.left();
    if (align & Qt::AlignRight)
        x = box.right() - w;
    else if (align & Qt::AlignHCenter)
        x = box.left() + (box.width() - w) / 2.0;

    if (rtl) {
        QTextOption option;
        option.setTextDirection(Qt::RightToLeft);
        option.setWrapMode(QTextOption::NoWrap);
        // The rect form is what takes a QTextOption; it is given the text's own advance
        // width so the glyphs land exactly where the point form would have put them.
        p->drawText(QRectF(x, baselineY - fm.ascent(), w, fm.height()), s, option);
    } else {
        p->drawText(QPointF(x, baselineY), s);
    }
    p->restore();
}

void drawCentered(QPainter *p, const QRectF &box, const QFont &f, const QColor &c,
                  const QString &text, Qt::Alignment align, bool elide)
{
    drawText(p, box, centeredBaseline(f, box), f, c, text, align, elide);
}

QStringList wrapLines(const QFont &f, const QString &text, qreal width, qreal firstWidth)
{
    if (text.isEmpty() || width <= 0.0)
        return {text};

    // The layout only finds the break points; each line is drawn afterwards through
    // drawText, so it lands on the same baselines and gets the same bidi handling as
    // every other run of text here. Breaking anywhere is the fallback for a word wider
    // than the line, never the first choice — see the header.
    QTextLayout layout(text, f);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    if (Locale::isRtl())
        option.setTextDirection(Qt::RightToLeft);
    layout.setTextOption(option);

    QStringList lines;
    layout.beginLayout();
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        const bool first = lines.isEmpty();
        line.setLineWidth(qMax(1.0, (first && firstWidth > 0.0) ? firstWidth : width));
        lines << text.mid(line.textStart(), line.textLength()).trimmed();
    }
    layout.endLayout();

    if (lines.isEmpty())
        lines << text;
    return lines;
}

QString upperTr(const QString &s)
{
    // Only Turkish needs the dotted/dotless pair done by hand — QString::toUpper() is
    // locale independent and would turn "i" into "I" and leave "ı" alone, both wrong for
    // Turkish. Applying that substitution to any other language is its own bug: English
    // "Firmware" has an "i" too, and the same rule would capitalise it as "FİRMWARE"
    // with a Turkish dotted İ instead of the plain "FIRMWARE" every other language wants.
    if (Locale::language() != QLatin1String("tr"))
        return s.toUpper();

    QString out = s;
    out.replace(QChar(u'i'), QChar(u'İ'));
    out.replace(QChar(u'ı'), QChar(u'I'));
    return out.toUpper();
}

qreal rowPadY()
{
    return Theme::compact() ? 4.0 : 7.0;
}

qreal rowNameLine()
{
    return normalLine(Theme::Font::tweakName());
}

qreal rowDescLine()
{
    return line(Theme::Font::tweakDesc(), 1.45);
}

int flexColumns(qreal available, qreal cell, qreal gap, int count)
{
    if (count <= 0)
        return 0;
    // A cell of no width would divide by zero below, and a caller asking to wrap nothing
    // wants every cell on one line anyway.
    if (cell <= 0.0)
        return count;

    // n cells carry n-1 gaps, so n*cell + (n-1)*gap <= available rearranges to this.
    int fit = int((available + gap) / (cell + gap));
    fit = qBound(1, fit, count);

    // The rebalance the header describes: keep the number of rows that many columns
    // implies, then spread the cells evenly over exactly those rows.
    const int rows = (count + fit - 1) / fit;
    return (count + rows - 1) / rows;
}

void hairline(QPainter *p, const QRectF &r, const QColor &c)
{
    // The contract in the header is a crisp 1 *logical* pixel, and a bare fillRect does
    // not deliver it. With QPainter::Antialiasing on — which most of the callers have,
    // since they are drawing rounded cards in the same pass — a rect whose edge falls on
    // a fractional coordinate is blended across two rows of device pixels at partial
    // alpha, so the "1px" divider comes out softer than the one drawn two lines earlier
    // at a whole coordinate. Dividers looked unevenly weighted for exactly this reason.
    //
    // Two things fix it, and neither needs the painter's transform unpicked: round the
    // thin axis to a whole logical pixel, and turn antialiasing off for the fill so the
    // raster engine lands on device pixels instead of blending across them.
    QRectF line = r;
    if (r.height() <= r.width()) {   // horizontal rule
        line.moveTop(std::round(r.top()));
        line.setHeight(std::max(1.0, std::round(r.height())));
    } else {                         // vertical rule
        line.moveLeft(std::round(r.left()));
        line.setWidth(std::max(1.0, std::round(r.width())));
    }

    const bool wasAntialiased = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, false);
    p->fillRect(line, c);
    p->setRenderHint(QPainter::Antialiasing, wasAntialiased);
}

} // namespace Css
