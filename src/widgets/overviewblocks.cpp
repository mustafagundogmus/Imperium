#include "overviewblocks.h"
#include "sectionheader.h"
#include "../css.h"
#include "../theme.h"

#include <QPainter>
#include <QtMath>

namespace {
constexpr qreal TilePadX = 12.0;
constexpr qreal TilePadY = 10.0;
constexpr qreal TileGap = 3.0;

constexpr qreal RowPadX = 6.0;
constexpr qreal RowPadY = 5.0;
constexpr qreal RowGap = 16.0;
} // namespace

// -------------------------------------------------------------------- StatTile ---

StatTile::StatTile(const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_label(Css::upperTr(label))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFixedHeight(sizeHint().height());
}

void StatTile::setValue(const QString &value)
{
    if (m_value == value)
        return;
    m_value = value;
    update();
}

void StatTile::setSub(const QString &sub)
{
    if (m_sub == sub)
        return;
    m_sub = sub;
    update();
}

QSize StatTile::sizeHint() const
{
    const qreal h = 2 /*border*/ + 2 * TilePadY
                    + Css::normalLine(Theme::Font::tileLabel()) + TileGap
                    + Css::normalLine(Theme::Font::tileValue()) + TileGap
                    + Css::normalLine(Theme::Font::tileSub());
    return {0, qRound(h)};
}

void StatTile::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.setPen(QPen(Color::TileBorder(), 1.0));
    p.setBrush(Color::Tile());
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0),
                      Metric::ControlRadius, Metric::ControlRadius);

    const QFont &labelFont = Font::tileLabel();
    const QFont &valueFont = Font::tileValue();
    const QFont &subFont = Font::tileSub();

    const QRectF box(1 + TilePadX, 0, width() - 2 * (1 + TilePadX), height());

    qreal top = 1 + TilePadY;
    const qreal labelLine = Css::normalLine(labelFont);
    Css::drawText(&p, box, Css::baseline(labelFont, top, labelLine), labelFont,
                  Color::TextDim(), m_label, Qt::AlignLeft, true);

    top += labelLine + TileGap;
    const qreal valueLine = Css::normalLine(valueFont);
    Css::drawText(&p, box, Css::baseline(valueFont, top, valueLine), valueFont,
                  Color::TextPrimary(), m_value, Qt::AlignLeft, true);

    top += valueLine + TileGap;
    const qreal subLine = Css::normalLine(subFont);
    Css::drawText(&p, box, Css::baseline(subFont, top, subLine), subFont,
                  Color::TextFaint(), m_sub, Qt::AlignLeft, true);
}

// ----------------------------------------------------------------- InfoSection ---

InfoSection::InfoSection(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_header = new SectionHeader(title, this);
}

qreal InfoSection::rowHeight()
{
    // `align-items:baseline`: the row is as tall as the deepest ascent plus the
    // deepest descent among its children, not the taller of the two line boxes.
    const QFont &labelFont = Theme::Font::infoLabel();
    const QFont &monoFont = Theme::Font::infoValueMono();
    const QFont &textFont = Theme::Font::infoValueText();

    const qreal above = qMax(Css::ascent(labelFont), qMax(Css::ascent(monoFont), Css::ascent(textFont)));
    const qreal below = qMax(Css::descent(labelFont), qMax(Css::descent(monoFont), Css::descent(textFont)));

    return 2 * RowPadY + above + below + 1.0 /*border-bottom*/;
}

void InfoSection::setRows(const QVector<InfoRow> &rows)
{
    m_rows = rows;
    setFixedHeight(sizeHint().height());
    update();
}

void InfoSection::setRowValue(int index, const QString &value)
{
    if (index < 0 || index >= m_rows.size() || m_rows.at(index).value == value)
        return;
    m_rows[index].value = value;
    update();
}

QSize InfoSection::sizeHint() const
{
    return {0, qRound(m_header->sizeHint().height() + m_rows.size() * rowHeight())};
}

void InfoSection::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    m_header->setGeometry(0, 0, width(), m_header->sizeHint().height());
}

void InfoSection::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont &labelFont = Font::infoLabel();
    const QFont &monoFont = Font::infoValueMono();
    const QFont &textFont = Font::infoValueText();

    const qreal above = qMax(Css::ascent(labelFont), qMax(Css::ascent(monoFont), Css::ascent(textFont)));
    const qreal rh = rowHeight();

    qreal y = m_header->sizeHint().height();
    for (const InfoRow &row : m_rows) {
        const QRectF box(RowPadX, y, width() - 2 * RowPadX, rh);
        const qreal baseline = y + RowPadY + above;

        const QFont &valueFont = row.mono ? monoFont : textFont;
        const qreal valueW = Css::textWidth(valueFont, row.value);

        Css::drawText(&p, QRectF(box.left(), box.top(), qMax(0.0, box.width() - valueW - RowGap), rh),
                      baseline, labelFont, Color::TextDesc(), row.label, Qt::AlignLeft, true);
        Css::drawText(&p, box, baseline, valueFont, Color::TextMono(), row.value, Qt::AlignRight);

        Css::hairline(&p, QRectF(RowPadX, y + rh - 1.0, width() - 2 * RowPadX, 1.0),
                      Color::DividerSoft());
        y += rh;
    }
}
