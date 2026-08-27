#include "themeswitch.h"

#include "../css.h"
#include "../i18n.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <iterator>

namespace {

constexpr qreal CardW = 84.0;
constexpr qreal CardH = 52.0;
constexpr qreal Gap = 10.0;
constexpr qreal RingOffset = 3.0;   // the selection ring sits this far outside the card
constexpr qreal LabelGap = 5.0;
constexpr qreal RowVGap = 12.0;     // space below a label before the next row of cards
constexpr int Cols = 4;             // eight themes wrap to a 4×2 grid

const Theme::Appearance Order[] = {
    Theme::Appearance::Dark,   Theme::Appearance::Light,
    Theme::Appearance::Midnight, Theme::Appearance::Sepia,
    Theme::Appearance::Ocean,  Theme::Appearance::Forest,
    Theme::Appearance::Dusk,   Theme::Appearance::Rose};
constexpr int Count = int(std::size(Order));

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
    setFixedSize(sizeHint());
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedSize(sizeHint());
        update();
    });
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        setFixedSize(sizeHint());
        update();
    });
}

QSize ThemeSwitch::sizeHint() const
{
    const qreal labelLine = Css::normalLine(Theme::Font::tileSub());
    const int cols = qMin(Count, Cols);
    const int rows = (Count + Cols - 1) / Cols;
    const qreal rowPitch = CardH + LabelGap + labelLine + RowVGap;
    return {qRound(cols * CardW + (cols - 1) * Gap + 2 * RingOffset),
            qRound(rows * rowPitch - RowVGap + 2 * RingOffset)};
}

QRectF ThemeSwitch::cardRect(int index) const
{
    const qreal labelLine = Css::normalLine(Theme::Font::tileSub());
    const qreal rowPitch = CardH + LabelGap + labelLine + RowVGap;
    const int col = index % Cols;
    const int row = index / Cols;
    return {RingOffset + col * (CardW + Gap), RingOffset + row * rowPitch, CardW, CardH};
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

        const QRectF labelBox(card.left(), card.bottom() + LabelGap, card.width(), labelLine);
        Css::drawText(&p, labelBox, Css::baseline(labelFont, labelBox.top(), labelLine), labelFont,
                      selected ? Color::TextPrimary() : Color::TextFaint(),
                      labelFor(Order[i]), Qt::AlignHCenter);
    }
}
