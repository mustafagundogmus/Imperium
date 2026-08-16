#include "css.h"
#include "i18n.h"
#include "theme.h"

#include <QColor>
#include <QFontMetricsF>
#include <QPainter>

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

    QString s = text;
    const QFontMetricsF fm(f);
    qreal w = fm.horizontalAdvance(s);
    if (elide && w > box.width()) {
        s = fm.elidedText(s, Qt::ElideRight, box.width());
        w = fm.horizontalAdvance(s);
    }

    qreal x = box.left();
    if (align & Qt::AlignRight)
        x = box.right() - w;
    else if (align & Qt::AlignHCenter)
        x = box.left() + (box.width() - w) / 2.0;

    p->drawText(QPointF(x, baselineY), s);
    p->restore();
}

void drawCentered(QPainter *p, const QRectF &box, const QFont &f, const QColor &c,
                  const QString &text, Qt::Alignment align, bool elide)
{
    drawText(p, box, centeredBaseline(f, box), f, c, text, align, elide);
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

void hairline(QPainter *p, const QRectF &r, const QColor &c)
{
    p->fillRect(r, c);
}

} // namespace Css
