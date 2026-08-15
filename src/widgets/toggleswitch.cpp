#include "toggleswitch.h"
#include "../theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {

constexpr qreal BorderW = 1.0;
// Square, like the window itself. Anything rounded reads as a pill, which is the shape
// this design deliberately moved away from.
constexpr qreal TrackRadius = 0.0;
constexpr qreal KnobRadius = 0.0;
constexpr qreal KnobSize = 10.0;
constexpr qreal KnobInset = 2.0;
constexpr int SlideMs = 140;

QColor lerp(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF()   + (b.redF()   - a.redF())   * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF()  + (b.blueF()  - a.blueF())  * t);
}

} // namespace

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(Theme::Metric::ToggleWidth, Theme::Metric::ToggleHeight);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover, true);

    m_slide = new QVariantAnimation(this);
    m_slide->setDuration(SlideMs);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_t = v.toReal();
        update();
    });

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

QSize ToggleSwitch::sizeHint() const
{
    return {Theme::Metric::ToggleWidth, Theme::Metric::ToggleHeight};
}

QRectF ToggleSwitch::knobRect() const
{
    // The knob is inset from the padding box, i.e. from inside the 1px border.
    const qreal left = BorderW + KnobInset;
    const qreal right = width() - BorderW - KnobInset - KnobSize;
    const qreal top = std::round((height() - KnobSize) / 2.0);
    return {left + (right - left) * m_t, top, KnobSize, KnobSize};
}

void ToggleSwitch::setChecked(bool on, bool animate)
{
    if (m_checked == on)
        return;
    m_checked = on;

    m_slide->stop();
    if (animate) {
        m_slide->setStartValue(m_t);
        m_slide->setEndValue(on ? 1.0 : 0.0);
        m_slide->start();
    } else {
        m_t = on ? 1.0 : 0.0;
        update();
    }
    Q_EMIT toggled(on);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor accent = Theme::accent();
    const qreal fill = qBound(0.0, m_t, 1.0);
    const QRectF track(BorderW / 2.0, BorderW / 2.0, width() - BorderW, height() - BorderW);
    const QRectF knob = knobRect();

    QPainterPath box;
    box.addRoundedRect(track, TrackRadius, TrackRadius);

    p.setPen(Qt::NoPen);
    p.setBrush(Color::ToggleOff());
    p.drawPath(box);

    // The accent fills in behind the knob rather than cross-fading, so the colour looks
    // carried by the knob instead of appearing underneath it. The edge runs slightly
    // past the knob as it travels so that at the end the fill reaches the far side —
    // stopping at the knob leaves an unfilled sliver in the corner.
    if (fill > 0.0) {
        p.save();
        p.setClipPath(box);
        p.setBrush(accent);
        p.drawRect(QRectF(0, 0, knob.right() + KnobInset * fill, height()));
        p.restore();
    }

    QColor border = lerp(Color::ToggleOffBorder(), accent, fill);
    if (hasFocus())
        border = Theme::accentInk();
    else if (m_hovered)
        border = lerp(border, Color::TextMuted(), 0.35);

    p.setPen(QPen(border, BorderW));
    p.setBrush(Qt::NoBrush);
    p.drawPath(box);

    QColor knobColour = lerp(Color::KnobOff(), Color::KnobOn(), fill);
    if (m_pressed)
        knobColour = lerp(knobColour, Color::TextPrimary(), 0.25);

    p.setPen(Qt::NoPen);
    p.setBrush(knobColour);
    p.drawRoundedRect(knob, KnobRadius, KnobRadius);
}

void ToggleSwitch::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void ToggleSwitch::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void ToggleSwitch::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const bool wasPressed = m_pressed;
        m_pressed = false;
        update();
        e->accept();
        if (wasPressed && rect().contains(e->pos()))
            setChecked(!m_checked);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void ToggleSwitch::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        setChecked(!m_checked);
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}
