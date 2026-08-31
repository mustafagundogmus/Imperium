#include "overviewblocks.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <cmath>

namespace {

constexpr qreal TilePadX = 12.0;
constexpr qreal TilePadY = 10.0;
constexpr qreal TileGap = 3.0;

constexpr qreal CardPadX = 12.0;   // InfoSection card
constexpr qreal CardPadTop = 10.0;
constexpr qreal CardPadBottom = 9.0;
constexpr qreal TitleGap = 8.0;    // title baseline box → rule
constexpr qreal RuleGap = 6.0;     // rule → first row
constexpr qreal TitleIconGap = 8.0;   // glyph → title
constexpr qreal TitleNoteGap = 10.0;  // title → the note on the right, when there is one

constexpr qreal RowPadY = 5.0;
constexpr qreal RowGap = 16.0;     // label ↔ value

constexpr qreal MeterHeight = 3.0;
constexpr qreal MeterGap = 5.0;
constexpr qreal StackGap = 3.0;   // label ↕ value on a stacked row

/// The bar both blocks draw: a rounded track with a rounded fill in the accent.
void drawMeter(QPainter *p, const QRectF &box, qreal fraction)
{
    using namespace Theme;

    const qreal radius = box.height() / 2.0;
    p->setPen(Qt::NoPen);
    p->setBrush(Color::ToggleOff());
    p->drawRoundedRect(box, radius, radius);

    const qreal width = box.width() * qBound(0.0, fraction, 1.0);
    if (width <= 0.0)
        return;

    // Anything narrower than the cap would render as a lozenge rather than a bar, so a
    // nearly-empty meter is clipped to the track instead of drawn short and fat.
    QPainterPath clip;
    clip.addRoundedRect(box, radius, radius);
    p->save();
    p->setClipPath(clip);
    p->setBrush(accent());
    p->drawRoundedRect(QRectF(box.left(), box.top(), qMax(width, box.height()), box.height()),
                       radius, radius);
    p->restore();
}

} // namespace

// -------------------------------------------------------------------- StatTile ---

StatTile::StatTile(const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_label(Css::upperTr(label))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFixedHeight(sizeHint().height());
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedHeight(sizeHint().height());
        update();
    });
}

void StatTile::setLabel(const QString &label)
{
    const QString upper = Css::upperTr(label);
    if (m_label == upper)
        return;
    m_label = upper;
    update();
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

void StatTile::setMeter(qreal fraction)
{
    if (qFuzzyCompare(m_meter + 2.0, fraction + 2.0))
        return;
    m_meter = fraction;
    update();
}

QSize StatTile::sizeHint() const
{
    const qreal h = 2 /*border*/ + 2 * TilePadY
                    + Css::normalLine(Theme::Font::tileLabel()) + TileGap
                    + Css::normalLine(Theme::Font::tileValue())
                    + MeterGap + MeterHeight + MeterGap
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

    top += valueLine + MeterGap;
    if (m_meter >= 0.0)
        drawMeter(&p, QRectF(box.left(), top, box.width(), MeterHeight), m_meter);

    top += MeterHeight + MeterGap;
    const qreal subLine = Css::normalLine(subFont);
    Css::drawText(&p, box, Css::baseline(subFont, top, subLine), subFont,
                  Color::TextFaint(), m_sub, Qt::AlignLeft, true);
}

// ----------------------------------------------------------------- InfoSection ---

InfoSection::InfoSection(const QString &title, const Icons::Glyph &icon, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_icon(&icon)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // Grows to the tallest card in its grid row rather than stopping at its own content:
    // three cards of three different heights side by side read as debris, and the rows
    // are drawn from the top anyway, so the extra height is just the card breathing.
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setMinimumHeight(sizeHint().height());
        updateGeometry();
        update();
    });
}

qreal InfoSection::headerHeight()
{
    return CardPadTop + Css::normalLine(Theme::Font::blockTitle()) + TitleGap + 1.0 + RuleGap;
}

qreal InfoSection::rowHeight(bool withMeter, bool stacked)
{
    // `align-items:baseline`: the row is as tall as the deepest ascent plus the
    // deepest descent among its children, not the taller of the two line boxes.
    const QFont &labelFont = Theme::Font::infoLabel();
    const QFont &monoFont = Theme::Font::infoValueMono();
    const QFont &textFont = Theme::Font::infoValueText();

    const qreal above = qMax(Css::ascent(labelFont), qMax(Css::ascent(monoFont), Css::ascent(textFont)));
    const qreal below = qMax(Css::descent(labelFont), qMax(Css::descent(monoFont), Css::descent(textFont)));

    const qreal meter = withMeter ? MeterGap + MeterHeight : 0.0;
    // A stacked row is two line boxes with the same padding around the pair, plus the
    // gap between them.
    const qreal second = stacked ? above + below + StackGap : 0.0;
    return 2 * RowPadY + above + below + second + meter + 1.0 /*separator*/;
}

void InfoSection::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    update();
}

void InfoSection::setRows(const QVector<InfoRow> &rows)
{
    m_rows = rows;
    setMinimumHeight(sizeHint().height());
    updateGeometry();
    update();
}

void InfoSection::setRowValue(int index, const QString &value)
{
    if (index < 0 || index >= m_rows.size() || m_rows.at(index).value == value)
        return;
    m_rows[index].value = value;
    update();
}

void InfoSection::setNote(const QString &note)
{
    if (m_note == note)
        return;
    m_note = note;
    update();
}

qreal InfoSection::contentHeight() const
{
    qreal h = 0.0;
    for (const InfoRow &row : m_rows)
        h += rowHeight(row.meter >= 0.0, row.stacked);
    return h;
}

QSize InfoSection::sizeHint() const
{
    return {0, qRound(headerHeight() + contentHeight() + CardPadBottom)};
}

void InfoSection::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.setPen(QPen(Color::TileBorder(), 1.0));
    p.setBrush(Color::Tile());
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0),
                      Metric::ControlRadius, Metric::ControlRadius);

    const qreal left = CardPadX;
    const qreal right = width() - CardPadX;

    // --- title -----------------------------------------------------------------
    const QFont &titleFont = Font::blockTitle();
    const qreal titleLine = Css::normalLine(titleFont);
    const qreal titleBaseline = Css::baseline(titleFont, CardPadTop, titleLine);

    // The glyph is the title's own line box, which is the one measurement that cannot put
    // it into the rule below and still follows all six faces and all four interface scales
    // — 11, 13, 14 and 16px, measured across every face in the build. It was written as a
    // multiple of the title's pixel size capped at this line, but no face in the build has
    // a line box that big (they run about 0.98em), so the cap bound every time and the
    // multiple never once decided anything.
    const int iconSize = qRound(titleLine);
    // Rounded to a whole pixel so the glyph's own 1px strokes land on the device grid the
    // way the rest of the card's hairlines do.
    const qreal iconY = std::round(CardPadTop + (titleLine - iconSize) / 2.0);
    // The colour is read here, at paint time, and Icons:: caches per colour — which is all
    // that following the theme takes: MainWindow repaints the tree on appearanceChanged,
    // this asks for TextPrimary again, and the cache answers with the right palette's copy.
    p.drawPixmap(QPointF(left, iconY),
                 Icons::lucide(*m_icon, Color::TextPrimary(), iconSize, devicePixelRatioF()));

    const qreal titleLeft = left + iconSize + TitleIconGap;

    // The note is measured out of the title's width rather than drawn over it. It only
    // ever appears on the storage card, whose title is short, but the glyph and its gap
    // have just taken 21px off the line at 100% and more at every step above it, and a
    // title elided into the note would be the way that shows.
    const QFont &noteFont = Font::sectionCount();
    const qreal noteW = m_note.isEmpty() ? 0.0
                                         : Css::textWidth(noteFont, m_note) + TitleNoteGap;

    Css::drawText(&p, QRectF(titleLeft, 0, qMax(0.0, right - titleLeft - noteW), titleLine),
                  titleBaseline, titleFont, Color::TextPrimary(), m_title, Qt::AlignLeft, true);

    if (!m_note.isEmpty()) {
        Css::drawText(&p, QRectF(titleLeft, 0, right - titleLeft, titleLine), titleBaseline,
                      noteFont, Color::TextFainter(), m_note, Qt::AlignRight);
    }

    Css::hairline(&p, QRectF(left, CardPadTop + titleLine + TitleGap, right - left, 1.0),
                  Color::Divider());

    // --- rows ------------------------------------------------------------------
    const QFont &labelFont = Font::infoLabel();
    const QFont &monoFont = Font::infoValueMono();
    const QFont &textFont = Font::infoValueText();
    const qreal above = qMax(Css::ascent(labelFont), qMax(Css::ascent(monoFont), Css::ascent(textFont)));

    qreal y = headerHeight();
    for (int i = 0; i < m_rows.size(); ++i) {
        const InfoRow &row = m_rows.at(i);
        const bool metered = row.meter >= 0.0;
        const qreal rh = rowHeight(metered, row.stacked);
        const QRectF box(left, y, right - left, rh);
        const qreal baseline = y + RowPadY + above;

        const QFont &valueFont = row.mono ? monoFont : textFont;
        qreal lastBaseline = baseline;

        if (row.stacked) {
            // Two full-width lines: the name, then what is true about it. Both elided
            // against the card rather than against whatever the other one left over.
            Css::drawText(&p, box, baseline, labelFont, Color::TextPrimary(), row.label,
                          Qt::AlignLeft, true);
            lastBaseline = baseline + Css::descent(labelFont) + StackGap + Css::ascent(valueFont);
            Css::drawText(&p, box, lastBaseline, valueFont, Color::TextMono(), row.value,
                          Qt::AlignLeft, true);
        } else {
            const qreal valueW = Css::textWidth(valueFont, row.value);
            Css::drawText(&p, QRectF(box.left(), box.top(), qMax(0.0, box.width() - valueW - RowGap), rh),
                          baseline, labelFont, Color::TextDesc(), row.label, Qt::AlignLeft, true);
            // Elided against the card too: a value wider than the whole row used to paint
            // straight out through the left edge rather than stop at it.
            Css::drawText(&p, box, baseline, valueFont, Color::TextMono(), row.value,
                          Qt::AlignRight, true);
        }

        if (metered) {
            const qreal meterTop = lastBaseline + Css::descent(valueFont) + MeterGap;
            drawMeter(&p, QRectF(left, meterTop, right - left, MeterHeight), row.meter);
        }

        // No separator under the last row: the card's own edge closes the list.
        if (i + 1 < m_rows.size())
            Css::hairline(&p, QRectF(left, y + rh - 1.0, right - left, 1.0), Color::DividerSoft());
        y += rh;
    }
}
