#include "buttons.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {

/// CSS `filter: brightness(1.08)`.
QColor brighten(const QColor &c, qreal factor)
{
    return QColor::fromRgbF(qMin(1.0, c.redF() * factor),
                            qMin(1.0, c.greenF() * factor),
                            qMin(1.0, c.blueF() * factor),
                            c.alphaF());
}

} // namespace

// ------------------------------------------------------------------ PillButton ---

PillButton::PillButton(Variant variant, const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_variant(variant)
    , m_text(text)
{
    setCursor(Qt::ArrowCursor);   // the mockup keeps `cursor:default` on both buttons
    setAttribute(Qt::WA_Hover, true);
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    refreshGeometry();
}

QSize PillButton::sizeHint() const
{
    const QFont &f = (m_variant == Ghost) ? Theme::Font::buttonGhost() : Theme::Font::buttonAccent();
    const qreal padX = (m_variant == Ghost) ? 10.0 : 12.0;
    const qreal padY = (m_variant == Ghost) ? 3.0 : 4.0;
    const qreal border = (m_variant == Ghost) ? 2.0 : 0.0;

    return {qCeil(2 * padX + border + Css::textWidth(f, m_text)),
            qCeil(2 * padY + border + Css::normalLine(f))};
}

void PillButton::refreshGeometry()
{
    setFixedSize(sizeHint());
    // Every metric here comes out of the font, so the face changing means measuring again.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedSize(sizeHint());
        updateGeometry();
        update();
    });

}

void PillButton::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    refreshGeometry();
    update();
}

void PillButton::setEnabledLook(bool enabled)
{
    if (m_live == enabled)
        return;
    m_live = enabled;
    setCursor(Qt::ArrowCursor);
    update();
}

void PillButton::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void PillButton::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void PillButton::mousePressEvent(QMouseEvent *e)
{
    // Accept even when dimmed: "Uygula" sits within a few pixels of the window's bottom
    // right corner, and an ignored press there would reach FramelessWindow and start a
    // resize drag instead of a click.
    if (e->button() == Qt::LeftButton) {
        m_pressed = m_live;
        update();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void PillButton::mouseReleaseEvent(QMouseEvent *e)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;
    update();

    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (wasPressed && m_live && rect().contains(e->pos()))
            Q_EMIT clicked();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void PillButton::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal dim = m_live ? 1.0 : 0.45;

    if (m_variant == Ghost) {
        QColor border = Color::BorderControl();
        QColor fg = m_hovered && m_live ? Color::TextMono() : Color::TextMuted();
        border.setAlphaF(border.alphaF() * dim);
        fg.setAlphaF(fg.alphaF() * dim);

        p.setPen(QPen(border, 1.0));
        p.setBrush(m_pressed ? QBrush(Color::SurfaceHover()) : Qt::NoBrush);
        p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0),
                          Metric::ControlRadius, Metric::ControlRadius);
        Css::drawCentered(&p, rect(), Font::buttonGhost(), fg, m_text, Qt::AlignHCenter);
        return;
    }

    QColor bg = Theme::accent();
    if (m_live && m_pressed)
        bg = brighten(bg, 0.94);
    else if (m_live && m_hovered)
        bg = brighten(bg, 1.08);
    bg.setAlphaF(bg.alphaF() * dim);

    QColor fg = Color::OnAccent();
    fg.setAlphaF(fg.alphaF() * (m_live ? 1.0 : 0.7));

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0, 0, width(), height()),
                      Metric::ControlRadius, Metric::ControlRadius);
    Css::drawCentered(&p, rect(), Font::buttonAccent(), fg, m_text, Qt::AlignHCenter);
}

// ------------------------------------------------------------------- LinkLabel ---

LinkLabel::LinkLabel(const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_text(text)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setFixedSize(sizeHint());
    // Every metric here comes out of the font, so the face changing means measuring again.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedSize(sizeHint());
        updateGeometry();
        update();
    });

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
}

QSize LinkLabel::sizeHint() const
{
    const QFont &f = Theme::Font::link();
    return {qCeil(Css::textWidth(f, m_text)), qCeil(Css::normalLine(f))};
}

void LinkLabel::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void LinkLabel::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void LinkLabel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void LinkLabel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (rect().contains(e->pos()))
            Q_EMIT clicked();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void LinkLabel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const QFont &f = Theme::Font::link();
    const QColor c = m_hovered ? Theme::Color::LinkHover() : Theme::accentInk();
    Css::drawText(&p, rect(), Css::baseline(f, 0, Css::normalLine(f)), f, c, m_text);
}

// ---------------------------------------------------------------- WindowButton ---

WindowButton::WindowButton(Kind kind, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
{
    setFixedSize(Theme::Metric::WindowButtonWidth, Theme::Metric::TitleBarHeight);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_Hover, true);
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
}

QSize WindowButton::sizeHint() const
{
    return {Theme::Metric::WindowButtonWidth, Theme::Metric::TitleBarHeight};
}

void WindowButton::setKind(Kind kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    update();
}

void WindowButton::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void WindowButton::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void WindowButton::mousePressEvent(QMouseEvent *e)
{
    // The press MUST be accepted here. An ignored press propagates to TitleBar, which
    // answers it with startSystemMove(); the compositor then owns the drag and the
    // matching release never comes back — so the button would never fire.
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void WindowButton::mouseDoubleClickEvent(QMouseEvent *e)
{
    // Likewise: an ignored double click would reach TitleBar and toggle maximise.
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void WindowButton::mouseReleaseEvent(QMouseEvent *e)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;

    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (wasPressed && rect().contains(e->pos()))
            Q_EMIT clicked();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

QPixmap WindowButton::glyph() const
{
    const qreal dpr = devicePixelRatioF();
    switch (m_kind) {
    case Minimize: return Icons::windowMinimize(Theme::Color::IconStroke(), dpr);
    case Maximize: return Icons::windowMaximize(Theme::Color::IconStroke(), dpr);
    case Restore:  return Icons::windowRestore(Theme::Color::IconStroke(), dpr);
    case Close:    return Icons::windowClose(Theme::Color::IconStroke(), dpr);
    }
    return {};
}

void WindowButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    // The close button reads as the accent colour on hover instead of the fixed red every
    // other tweaker uses. The accent presets are all mid-light tones, so the usual neutral
    // glyph stroke would nearly disappear on them — the X switches to a dark tone only for
    // this state, just for that button.
    const bool closeHovered = m_hovered && m_kind == Close;
    if (m_hovered)
        p.fillRect(rect(), closeHovered ? Theme::accent() : Theme::Color::SurfaceActive());

    const QPixmap g = closeHovered
                          ? Icons::windowClose(QColor(0x1A, 0x1A, 0x1E), devicePixelRatioF())
                          : glyph();
    const QSizeF gs = g.deviceIndependentSize();
    p.drawPixmap(QPointF(std::round((width() - gs.width()) / 2.0),
                         std::round((height() - gs.height()) / 2.0)),
                 g);
}
