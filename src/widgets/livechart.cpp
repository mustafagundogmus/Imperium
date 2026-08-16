#include "livechart.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr qreal PadX = 12.0;
constexpr qreal PadY = 10.0;
constexpr qreal LegendGap = 8.0;     // between the legend row and the plot
constexpr qreal PlotHeight = 64.0;

constexpr qreal SwatchWidth = 10.0;
constexpr qreal SwatchHeight = 2.0;
constexpr qreal SwatchGap = 6.0;
constexpr qreal EntryGap = 18.0;
constexpr qreal NameValueGap = 7.0;

constexpr int GridRows = 4;          // 25 / 50 / 75 %
constexpr int GridSeconds = 15;      // one vertical rule every 15 samples

/// Memory uses the design's sage alternative so the two series stay distinguishable
/// without introducing a colour from outside the handoff's palette.
const QColor MemoryColour{0x7F, 0xB8, 0xA4};

} // namespace

LiveChart::LiveChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
    // Every metric here comes out of the font, so the face changing means measuring again.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedHeight(sizeHint().height());
        updateGeometry();
        update();
    });

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
}

QSize LiveChart::sizeHint() const
{
    const qreal legendLine = Css::normalLine(Theme::Font::tileSub());
    return {0, qRound(2 /*border*/ + 2 * PadY + legendLine + LegendGap + PlotHeight)};
}

void LiveChart::setSeries(const QVector<qreal> &cpu, const QVector<qreal> &ram, int capacity)
{
    m_cpu = cpu;
    m_ram = ram;
    m_capacity = qMax(2, capacity);
    update();
}

void LiveChart::paintSeries(QPainter &p, const QRectF &plot, const QVector<qreal> &values,
                            const QColor &colour, int capacity) const
{
    if (values.size() < 2)
        return;

    const qreal step = plot.width() / qreal(capacity - 1);
    const qreal firstX = plot.right() - step * (values.size() - 1);

    QPolygonF line;
    line.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        const qreal v = qBound(0.0, values.at(i), 100.0);
        line << QPointF(firstX + step * i, plot.bottom() - plot.height() * v / 100.0);
    }

    QPainterPath area;
    area.moveTo(line.first().x(), plot.bottom());
    for (const QPointF &pt : line)
        area.lineTo(pt);
    area.lineTo(line.last().x(), plot.bottom());
    area.closeSubpath();

    QLinearGradient fade(0, plot.top(), 0, plot.bottom());
    QColor top = colour;
    top.setAlpha(0x3A);
    QColor bottom = colour;
    bottom.setAlpha(0x00);
    fade.setColorAt(0.0, top);
    fade.setColorAt(1.0, bottom);

    p.setPen(Qt::NoPen);
    p.setBrush(fade);
    p.drawPath(area);

    p.setPen(QPen(colour, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);
}

void LiveChart::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.setPen(QPen(Color::TileBorder(), 1.0));
    p.setBrush(Color::Tile());
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0),
                      Metric::ControlRadius, Metric::ControlRadius);

    const QFont &nameFont = Font::tileSub();       // 10px sans
    const QFont &valueFont = Font::sectionCount(); // 9.5px mono
    const qreal legendLine = Css::normalLine(nameFont);

    const QRectF inner(1 + PadX, 1 + PadY, width() - 2 * (1 + PadX), height() - 2 * (1 + PadY));

    struct Entry
    {
        QString name;
        QColor colour;
        qreal value;
    };
    const QColor accent = Theme::accent();
    const Entry entries[] = {
        {Locale::tr(QStringLiteral("chart.islemci")), accent, m_cpu.isEmpty() ? 0.0 : m_cpu.last()},
        {Locale::tr(QStringLiteral("chart.bellek")), MemoryColour, m_ram.isEmpty() ? 0.0 : m_ram.last()},
    };

    qreal x = inner.left();
    const qreal legendBaseline = Css::baseline(nameFont, inner.top(), legendLine);
    for (const Entry &e : entries) {
        p.setPen(Qt::NoPen);
        p.setBrush(e.colour);
        p.drawRoundedRect(QRectF(x, legendBaseline - 4.0, SwatchWidth, SwatchHeight),
                          SwatchHeight / 2.0, SwatchHeight / 2.0);
        x += SwatchWidth + SwatchGap;

        Css::drawText(&p, QRectF(x, 0, inner.right() - x, height()), legendBaseline,
                      nameFont, Color::TextDim(), e.name);
        x += Css::textWidth(nameFont, e.name) + NameValueGap;

        const QString value = (e.value > 0.0 && e.value < 10.0)
                                  ? QStringLiteral("%%1").arg(QLocale().toString(e.value, 'f', 1))
                                  : QStringLiteral("%%1").arg(qRound(e.value));
        Css::drawText(&p, QRectF(x, 0, inner.right() - x, height()),
                      Css::baseline(valueFont, inner.top(), legendLine),
                      valueFont, Color::TextMono(), value);
        x += Css::textWidth(valueFont, value) + EntryGap;
    }

    const QString window = Locale::tr(QStringLiteral("chart.son")).arg(m_capacity);
    Css::drawText(&p, inner, Css::baseline(valueFont, inner.top(), legendLine), valueFont,
                  Color::TextFainter(), window, Qt::AlignRight);

    const QRectF plot(inner.left(), inner.top() + legendLine + LegendGap,
                      inner.width(), PlotHeight);

    for (int i = 1; i < GridRows; ++i) {
        const qreal y = std::round(plot.top() + plot.height() * i / GridRows) + 0.5;
        Css::hairline(&p, QRectF(plot.left(), y - 0.5, plot.width(), 1.0), Color::Divider());
    }
    for (int s = GridSeconds; s < m_capacity; s += GridSeconds) {
        const qreal gx = std::round(plot.right() - plot.width() * s / qreal(m_capacity - 1));
        Css::hairline(&p, QRectF(gx, plot.top(), 1.0, plot.height()), Color::DividerSoft());
    }

    p.save();
    p.setClipRect(plot.adjusted(-1, -1, 1, 1));
    paintSeries(p, plot, m_ram, MemoryColour, m_capacity);
    paintSeries(p, plot, m_cpu, accent, m_capacity);
    p.restore();

    Css::hairline(&p, QRectF(plot.left(), plot.bottom(), plot.width(), 1.0), Color::Divider());

    // A single point cannot be drawn as a line; say so instead of showing a blank grid.
    if (m_cpu.size() < 2)
        Css::drawCentered(&p, plot, nameFont, Color::TextFainter(),
                          Locale::tr(QStringLiteral("chart.veriToplaniyor")), Qt::AlignHCenter);
}
