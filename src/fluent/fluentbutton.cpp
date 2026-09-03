#include "fluentbutton.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {
constexpr qreal Radius = 4.0;
constexpr qreal IconGap = 8.0;
constexpr qreal ChevronGap = 8.0;
constexpr int ChevronSize = 12;
constexpr qreal ChevronStroke = 2.0;
const QString ChevronPath = QStringLiteral("m6 9 6 6 6-6");
} // namespace

FluentButton::FluentButton(Variant variant, const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_variant(variant)
    , m_text(text)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    refreshGeometry();

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, &FluentButton::refreshGeometry);
}

qreal FluentButton::padX() const
{
    return m_variant == Primary ? 18.0 : 14.0;
}

QFont FluentButton::font() const
{
    return Theme::sans(13, m_variant == Primary ? Theme::Weight::Medium : Theme::Weight::Regular);
}

QSize FluentButton::sizeHint() const
{
    qreal w = 2 * padX() + Css::textWidth(font(), m_text);
    if (!m_icon.isEmpty())
        w += m_iconSize + IconGap;
    if (m_chevron)
        w += ChevronGap + ChevronSize;
    return {qCeil(w), m_height};
}

void FluentButton::refreshGeometry()
{
    setFixedSize(sizeHint());
    updateGeometry();
    update();
}

void FluentButton::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    refreshGeometry();
}

void FluentButton::setLeadingIcon(const QString &pathData, int size, qreal stroke)
{
    m_icon = pathData;
    m_iconSize = size;
    m_iconStroke = stroke;
    refreshGeometry();
}

void FluentButton::setTrailingChevron(bool on)
{
    if (m_chevron == on)
        return;
    m_chevron = on;
    refreshGeometry();
}

void FluentButton::setButtonHeight(int height)
{
    if (m_height == height)
        return;
    m_height = height;
    refreshGeometry();
}

void FluentButton::setEnabledLook(bool enabled)
{
    if (m_live == enabled)
        return;
    m_live = enabled;
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void FluentButton::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void FluentButton::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void FluentButton::mousePressEvent(QMouseEvent *e)
{
    // Accepted even when dimmed: an ignored press reaches the frameless window and starts
    // a drag, and a dead button that moves the window is worse than one that does nothing.
    if (e->button() == Qt::LeftButton) {
        m_pressed = m_live;
        update();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void FluentButton::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const bool fire = m_pressed && m_live && rect().contains(e->pos());
        m_pressed = false;
        update();
        e->accept();
        if (fire)
            Q_EMIT clicked();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void FluentButton::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    if (!m_live)
        p.setOpacity(0.5);

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    const bool hot = m_live && m_hovered;
    QColor fg;
    if (m_variant == Primary) {
        QColor fill = t.accent;
        if (hot)
            fill = fill.lighter(108);   // filter: brightness(1.08)
        if (m_pressed)
            fill = fill.darker(106);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(0, 0, width(), height()), Radius, Radius);
        fg = t.onAccent;
    } else {
        p.setPen(QPen(t.controlBorder, 1.0));
        p.setBrush(hot ? t.controlHover : t.controlBg);
        p.drawRoundedRect(frame, Radius, Radius);
        fg = t.text;
    }

    const QFont f = font();
    qreal x = padX();
    if (!m_icon.isEmpty()) {
        const QPixmap icon = Icons::strokePath(m_icon, 24.0, QSize(m_iconSize, m_iconSize), fg,
                                               m_iconStroke, devicePixelRatioF());
        p.drawPixmap(QPointF(x, std::round((height() - m_iconSize) / 2.0)), icon);
        x += m_iconSize + IconGap;
    }
    const qreal textW = Css::textWidth(f, m_text);
    Css::drawCentered(&p, QRectF(x, 0, textW, height()), f, fg, m_text);
    x += textW;

    if (m_chevron) {
        x += ChevronGap;
        const QPixmap chevron = Icons::strokePath(ChevronPath, 24.0, QSize(ChevronSize, ChevronSize),
                                                  fg, ChevronStroke, devicePixelRatioF());
        p.drawPixmap(QPointF(x, std::round((height() - ChevronSize) / 2.0)), chevron);
    }
}
