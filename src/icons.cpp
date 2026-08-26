#include "icons.h"

#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

namespace Icons {
namespace {

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> c;
    return c;
}

QPixmap render(const QString &key, const QString &inner, qreal viewBox,
               const QSize &size, qreal dpr)
{
    const QString id = QStringLiteral("%1|%2x%3|%4").arg(key).arg(size.width()).arg(size.height()).arg(dpr);
    const auto it = cache().constFind(id);
    if (it != cache().cend())
        return *it;

    const QString doc = QStringLiteral(
                            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%1\" "
                            "viewBox=\"0 0 %1 %1\">%2</svg>")
                            .arg(viewBox)
                            .arg(inner);

    QSvgRenderer renderer(doc.toUtf8());
    QPixmap pm(QSize(qRound(size.width() * dpr), qRound(size.height() * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&p, QRectF(0, 0, size.width(), size.height()));
    }
    cache().insert(id, pm);
    return pm;
}

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

} // namespace

QPixmap strokePath(const QString &d, qreal viewBox, const QSize &size,
                   const QColor &c, qreal strokeWidth, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<path d=\"%1\" fill=\"none\" stroke=\"%2\" stroke-width=\"%3\" "
                              "stroke-linecap=\"round\" stroke-linejoin=\"round\"/>")
                              .arg(d, hex(c))
                              .arg(strokeWidth);
    return render(QStringLiteral("p:%1:%2:%3:%4").arg(d, hex(c)).arg(strokeWidth).arg(viewBox),
                  inner, viewBox, size, dpr);
}

QPixmap fragment(const QString &cacheKey, const QString &inner, qreal viewBox,
                 const QSize &size, qreal dpr)
{
    return render(cacheKey, inner, viewBox, size, dpr);
}

QPixmap search(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<circle cx=\"5.2\" cy=\"5.2\" r=\"3.4\" fill=\"none\" stroke=\"%1\" stroke-width=\"1.1\"/>"
                              "<path d=\"M8 8l2.6 2.6\" fill=\"none\" stroke=\"%1\" stroke-width=\"1.1\"/>")
                              .arg(hex(c));
    // The mockup renders a 12-unit viewBox into an 11×11 box.
    return fragment(QStringLiteral("search:%1").arg(hex(c)), inner, 12, QSize(11, 11), dpr);
}

QPixmap windowMinimize(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral("<path d=\"M1 5.5h8\" stroke=\"%1\" stroke-width=\"1\"/>").arg(hex(c));
    return fragment(QStringLiteral("wmin:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap windowMaximize(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<rect x=\"1.5\" y=\"1.5\" width=\"7\" height=\"7\" fill=\"none\" "
                              "stroke=\"%1\" stroke-width=\"1\"/>")
                              .arg(hex(c));
    return fragment(QStringLiteral("wmax:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap windowRestore(const QColor &c, qreal dpr)
{
    // Not in the mockup (which only ever shows the restored state) but required once the
    // maximise button actually works. Drawn in the same 10-unit grid and 1px stroke.
    const QString inner = QStringLiteral(
                              "<path d=\"M3 3V1.5h5.5V7H7\" fill=\"none\" stroke=\"%1\" stroke-width=\"1\"/>"
                              "<rect x=\"1.5\" y=\"3\" width=\"5.5\" height=\"5.5\" fill=\"none\" "
                              "stroke=\"%1\" stroke-width=\"1\"/>")
                              .arg(hex(c));
    return fragment(QStringLiteral("wres:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap windowClose(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral("<path d=\"M2 2l6 6M8 2l-6 6\" stroke=\"%1\" stroke-width=\"1\"/>").arg(hex(c));
    return fragment(QStringLiteral("wcls:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap sort(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<path d=\"M2 3.5h9M3.5 6.5h6M5 9.5h3\" fill=\"none\" stroke=\"%1\" "
                              "stroke-width=\"1.1\" stroke-linecap=\"round\"/>")
                              .arg(hex(c));
    return fragment(QStringLiteral("sort:%1").arg(hex(c)), inner, 13, QSize(13, 13), dpr);
}

QPixmap category(const QString &pathData, const QColor &c, qreal dpr)
{
    return strokePath(pathData, 12, QSize(12, 12), c, 1.0, dpr);
}

} // namespace Icons
