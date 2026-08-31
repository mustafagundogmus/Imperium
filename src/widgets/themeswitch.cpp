#include "themeswitch.h"

#include "../css.h"
#include "../i18n.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <iterator>

namespace {

constexpr qreal CardW = 84.0;
constexpr qreal CardH = 52.0;
// The one gallery gap, on both axes and shared with the chip grids beside this one. The
// vertical figure used to be a 12 of its own, which is the sort of one-off spacing that
// makes a page of otherwise identical parts read as several unrelated ones.
constexpr qreal Gap = 10.0;
constexpr qreal RingOffset = 3.0;   // the selection ring sits this far outside the card
constexpr qreal LabelGap = 5.0;

// The gallery's own order, which is Appearance's. The four light schemes are last for the
// reason given on the enum: appending leaves every card a user has already learnt the
// position of exactly where it was.
const Theme::Appearance Order[] = {
    Theme::Appearance::Dark,   Theme::Appearance::Light,
    Theme::Appearance::Midnight, Theme::Appearance::Sepia,
    Theme::Appearance::Ocean,  Theme::Appearance::Forest,
    Theme::Appearance::Dusk,   Theme::Appearance::Rose,
    Theme::Appearance::Mist,   Theme::Appearance::Contrast,
    Theme::Appearance::Meadow, Theme::Appearance::Lilac};
constexpr int Count = int(std::size(Order));
// Every scheme has a card, and the enum is what says how many there are. Order[] is a
// separate list that -Wswitch cannot check, so a scheme added to the enum and forgotten
// here would simply be unreachable from the settings page — visible only to somebody who
// counted the cards.
static_assert(Count == Theme::AppearanceCount,
              "every Appearance needs a card in Order[]");

QString labelFor(Theme::Appearance a)
{
    switch (a) {
    case Theme::Appearance::Light:    return Locale::tr(QStringLiteral("theme.light"));
    case Theme::Appearance::Midnight: return Locale::tr(QStringLiteral("theme.midnight"));
    case Theme::Appearance::Sepia:    return Locale::tr(QStringLiteral("theme.sepia"));
    case Theme::Appearance::Ocean:    return Locale::tr(QStringLiteral("theme.ocean"));
    case Theme::Appearance::Forest:   return Locale::tr(QStringLiteral("theme.forest"));
    case Theme::Appearance::Dusk:     return Locale::tr(QStringLiteral("theme.dusk"));
    case Theme::Appearance::Rose:     return Locale::tr(QStringLiteral("theme.rose"));
    case Theme::Appearance::Mist:     return Locale::tr(QStringLiteral("theme.mist"));
    case Theme::Appearance::Contrast: return Locale::tr(QStringLiteral("theme.contrast"));
    case Theme::Appearance::Meadow:   return Locale::tr(QStringLiteral("theme.meadow"));
    case Theme::Appearance::Lilac:    return Locale::tr(QStringLiteral("theme.lilac"));
    case Theme::Appearance::Dark:     break;
    }
    return Locale::tr(QStringLiteral("theme.dark"));
}

} // namespace

ThemeSwitch::ThemeSwitch(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    // Flows into the width it is handed and reports the height that leaves, the same
    // contract the language and typeface grids keep — Preferred and not Fixed vertically
    // for the reason spelled out in languagepicker.cpp: Fixed caps the item at one row's
    // height and clips the wrap.
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    // The label under a card is the only part of this widget measured from the font, and
    // it decides the height of a row of cards — so both of the things that can change it
    // have to ask the layout for a new height as well as repainting.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        updateGeometry();
        update();
    });
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        updateGeometry();
        update();
    });
}

namespace {

/// A card plus the label under it — the cell the grid is built from.
qreal cellHeight()
{
    return CardH + LabelGap + Css::normalLine(Theme::Font::tileSub());
}

} // namespace

int ThemeSwitch::columns() const
{
    return qMax(1, Css::flexColumns(width() - 2 * RingOffset, CardW, Gap, Count));
}

QSize ThemeSwitch::sizeHint() const
{
    // The shape it would rather have: all twelve palettes on one line. Nothing but a
    // maximised window has room for that any more — twelve cards want 1124px and the
    // settings page's content column is 970 at the design's own 1240px width — so in
    // practice this is the hint a wider container is allowed to honour, and everywhere
    // else heightForWidth() wraps rather than clipping. It is deliberately not narrowed to
    // what the settings page happens to give: a maximised window does have the room, and a
    // gallery that stays two rows wide on a 1920px desktop for no reason is the bug the
    // flexColumns rebalance exists to avoid.
    return {qCeil(Count * CardW + (Count - 1) * Gap + 2 * RingOffset),
            qCeil(cellHeight() + 2 * RingOffset)};
}

int ThemeSwitch::heightForWidth(int w) const
{
    const int cols = qMax(1, Css::flexColumns(w - 2 * RingOffset, CardW, Gap, Count));
    const int rows = (Count + cols - 1) / cols;
    return qCeil(rows * cellHeight() + (rows - 1) * Gap + 2 * RingOffset);
}

QRectF ThemeSwitch::cardRect(int index) const
{
    const int cols = columns();
    const int col = index % cols;
    const int row = index / cols;
    return {RingOffset + col * (CardW + Gap), RingOffset + row * (cellHeight() + Gap),
            CardW, CardH};
}

int ThemeSwitch::indexAt(const QPointF &pos) const
{
    for (int i = 0; i < Count; ++i)
        if (cardRect(i).adjusted(-RingOffset, -RingOffset, RingOffset, RingOffset).contains(pos))
            return i;
    return -1;
}

void ThemeSwitch::mouseMoveEvent(QMouseEvent *e)
{
    const int hit = indexAt(e->position());
    if (hit != m_hovered) {
        m_hovered = hit;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void ThemeSwitch::leaveEvent(QEvent *e)
{
    if (m_hovered != -1 || m_pressed != -1) {
        m_hovered = m_pressed = -1;
        update();
    }
    QWidget::leaveEvent(e);
}

void ThemeSwitch::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_pressed = indexAt(e->position());
    update();
}

void ThemeSwitch::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    const int hit = indexAt(e->position());
    if (hit >= 0 && hit == m_pressed)
        Q_EMIT picked(Order[hit]);
    m_pressed = -1;
    update();
}

/// A window in 84×52: title bar with its three dots, sidebar with a selected row, and a
/// content column of rules. Everything is drawn from the palette it is previewing, so
/// the two cards differ in exactly the way the app will.
void ThemeSwitch::paintPreview(QPainter *p, const QRectF &box, Theme::Appearance appearance) const
{
    const Theme::Palette &pal = Theme::palette(appearance);
    const QColor accent = Theme::accent();

    p->save();
    QPainterPath clip;
    clip.addRoundedRect(box, 5.0, 5.0);
    p->setClipPath(clip);

    p->fillRect(box, pal.window);

    // title bar
    const qreal barH = 9.0;
    p->fillRect(QRectF(box.left(), box.top(), box.width(), barH), pal.surfaceActive);
    for (int i = 0; i < 3; ++i) {
        p->setPen(Qt::NoPen);
        p->setBrush(pal.iconStroke);
        p->drawEllipse(QPointF(box.right() - 7.0 - i * 6.0, box.top() + barH / 2.0), 1.2, 1.2);
    }

    // sidebar, with the selected row in the accent wash
    const qreal sideW = 26.0;
    const QRectF side(box.left(), box.top() + barH, sideW, box.height() - barH);
    p->fillRect(side, pal.surface);

    // The same token the sidebar paints with, asked for the appearance this card is
    // offering. Re-deriving it here is what let the two drift apart.
    const QColor wash = Theme::accentSoft(appearance);
    p->fillRect(QRectF(side.left() + 3.0, side.top() + 4.0, sideW - 6.0, 5.0), wash);
    for (int i = 1; i < 4; ++i)
        p->fillRect(QRectF(side.left() + 3.0, side.top() + 4.0 + i * 8.0, sideW - 9.0, 2.0),
                    pal.textFaint);

    // content: a header rule, then rows, then an accent button
    const qreal cx = side.right() + 5.0;
    const qreal cw = box.right() - cx - 5.0;
    p->fillRect(QRectF(cx, box.top() + barH + 5.0, cw * 0.55, 3.0), pal.textSecondary);
    for (int i = 0; i < 3; ++i)
        p->fillRect(QRectF(cx, box.top() + barH + 13.0 + i * 7.0, cw, 2.0), pal.divider);

    p->setPen(Qt::NoPen);
    p->setBrush(accent);
    p->drawRoundedRect(QRectF(box.right() - 20.0, box.bottom() - 10.0, 14.0, 5.0), 2.0, 2.0);

    p->restore();

    p->setPen(QPen(pal.borderWindow, 1.0));
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 5.0, 5.0);
}

void ThemeSwitch::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont &labelFont = Font::tileSub();
    const qreal labelLine = Css::normalLine(labelFont);

    for (int i = 0; i < Count; ++i) {
        const QRectF card = cardRect(i);
        const bool selected = appearance() == Order[i];

        if (m_pressed == i || (m_hovered == i && !selected)) {
            p.setPen(Qt::NoPen);
            p.setBrush(Color::SurfaceHover());
            p.drawRoundedRect(card.adjusted(-RingOffset, -RingOffset, RingOffset, RingOffset),
                              7.0, 7.0);
        }

        paintPreview(&p, card, Order[i]);

        if (selected) {
            p.setPen(QPen(accent(), 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(card.adjusted(-RingOffset + 0.5, -RingOffset + 0.5,
                                            RingOffset - 0.5, RingOffset - 0.5),
                              7.0, 7.0);
        }

        // Elided to the card it belongs to, which matters more than it looks. Measuring
        // all twelve names in all ten languages across the six faces and the four text
        // sizes, the widest is the Arabic "منتصف الليل" — midnight — at 83.0px in Open
        // Sans at the largest size, inside an 84px card. One pixel. The four added last
        // are nowhere near it: the widest of them is "Contraste" at 60px, so they cost
        // nothing here, but the claim this comment used to make — that no label comes
        // close, the widest being the French "Crépuscule" — was wrong on both counts and
        // is worth not repeating. A thirteenth name a little longer than midnight's will
        // elide, and eliding is the right failure: a label that outgrew its card would run
        // under its neighbour rather than stop, and a grid whose labels overlap is worse
        // than one that trims.
        const QRectF labelBox(card.left(), card.bottom() + LabelGap, card.width(), labelLine);
        Css::drawText(&p, labelBox, Css::baseline(labelFont, labelBox.top(), labelLine), labelFont,
                      selected ? Color::TextPrimary() : Color::TextFaint(),
                      labelFor(Order[i]), Qt::AlignHCenter, true);
    }
}
