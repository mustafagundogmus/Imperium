#include "toggleswitch.h"
#include "../theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QVariantAnimation>

namespace {

constexpr qreal BorderW = 1.0;
constexpr qreal KnobInset = 2.0;
constexpr int SlideMs = 240;
constexpr int SquashMs = 130;
constexpr qreal SquashAmount = 0.34;   ///< how much wider than tall a held knob gets

QColor lerp(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF()   + (b.redF()   - a.redF())   * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF()  + (b.blueF()  - a.blueF())  * t);
}

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

} // namespace

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(Theme::Metric::ToggleWidth + 2 * Theme::Metric::ToggleBleed,
                 Theme::Metric::ToggleHeight + 2 * Theme::Metric::ToggleBleed);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover, true);

    m_slide = new QVariantAnimation(this);
    m_slide->setDuration(SlideMs);
    connect(m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_t = v.toReal();
        update();
    });

    m_squashAnim = new QVariantAnimation(this);
    m_squashAnim->setDuration(SquashMs);
    m_squashAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_squashAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_squash = v.toReal();
        update();
    });

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

QSize ToggleSwitch::sizeHint() const
{
    return {Theme::Metric::ToggleWidth + 2 * Theme::Metric::ToggleBleed,
            Theme::Metric::ToggleHeight + 2 * Theme::Metric::ToggleBleed};
}

/// The capsule inside the (larger) widget. Everything else is measured from this.
QRectF ToggleSwitch::capsule() const
{
    const qreal bleed = Theme::Metric::ToggleBleed;
    return {bleed + BorderW / 2.0, bleed + BorderW / 2.0,
            qreal(Theme::Metric::ToggleWidth) - BorderW,
            qreal(Theme::Metric::ToggleHeight) - BorderW};
}

qreal ToggleSwitch::knobRadius() const
{
    return (Theme::Metric::ToggleHeight - 2 * (BorderW + KnobInset)) / 2.0;
}

qreal ToggleSwitch::knobCentre() const
{
    const QRectF track = capsule();
    const qreal r = knobRadius();
    const qreal left = track.left() + BorderW / 2.0 + KnobInset + r;
    const qreal right = track.right() - BorderW / 2.0 - KnobInset - r;
    return left + (right - left) * m_t;
}

void ToggleSwitch::animateTo(qreal target, bool overshoot)
{
    m_slide->stop();

    // Turning on overshoots slightly so it snaps into place; turning off just settles,
    // which reads as letting go rather than springing back.
    if (overshoot) {
        QEasingCurve curve(QEasingCurve::OutBack);
        curve.setOvershoot(1.9);
        m_slide->setEasingCurve(curve);
    } else {
        m_slide->setEasingCurve(QEasingCurve::OutCubic);
    }

    m_slide->setStartValue(m_t);
    m_slide->setEndValue(target);
    m_slide->start();
}

void ToggleSwitch::setChecked(bool on, bool animate)
{
    if (m_checked == on)
        return;
    m_checked = on;

    if (animate) {
        animateTo(on ? 1.0 : 0.0, on);
    } else {
        m_slide->stop();
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
    const QRectF track = capsule();
    const qreal radius = track.height() / 2.0;
    const qreal fill = qBound(0.0, m_t, 1.0);
    const qreal cx = knobCentre();
    const qreal cy = track.center().y();

    // --- halo -------------------------------------------------------------------
    // A soft accent bloom under the track. The only non-flat fill in the whole app,
    // and what makes an enabled switch read from across the window.
    if (fill > 0.01) {
        // Reaches exactly to the widget edge, so the bloom fades to nothing instead of
        // being cut off — a clipped radial gradient reads as a tinted rectangle.
        const qreal reach = Theme::Metric::ToggleBleed + Theme::Metric::ToggleHeight / 2.0;
        QRadialGradient halo(QPointF(cx, cy), reach);
        halo.setColorAt(0.0, withAlpha(accent, int(70 * fill)));
        halo.setColorAt(1.0, withAlpha(accent, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(QPointF(cx, cy), reach, reach);
    }

    // --- track ------------------------------------------------------------------
    QPainterPath capsule;
    capsule.addRoundedRect(track, radius, radius);

    p.setPen(Qt::NoPen);
    p.setBrush(Color::ToggleOff());
    p.drawPath(capsule);

    // The accent wipes across rather than cross-fading: the fill edge tracks the knob,
    // so the colour looks carried by the knob instead of faded in underneath it.
    if (fill > 0.0) {
        p.save();
        p.setClipPath(capsule);
        p.setBrush(accent);
        p.drawRect(QRectF(track.left(), track.top(), cx + knobRadius() - track.left(), track.height()));
        p.restore();
    }

    p.setPen(QPen(lerp(Color::ToggleOffBorder(), accent, fill), BorderW));
    p.setBrush(Qt::NoBrush);
    p.drawPath(capsule);

    // --- knob -------------------------------------------------------------------
    const qreal r = knobRadius();
    const qreal grow = m_hovered ? 0.5 : 0.0;
    const qreal stretch = r * SquashAmount * m_squash;

    // The knob widens towards where it is heading, so a press feels physical: the
    // trailing edge stays put and the leading edge runs ahead.
    const qreal trailing = m_checked ? 0.0 : stretch;
    const QRectF knob(cx - r - grow - trailing, cy - r - grow,
                      2 * (r + grow) + stretch, 2 * (r + grow));

    p.setPen(Qt::NoPen);
    p.setBrush(lerp(Color::KnobOff(), Color::KnobOn(), fill));
    p.drawRoundedRect(knob, r + grow, r + grow);

    // --- focus ------------------------------------------------------------------
    if (hasFocus()) {
        p.setPen(QPen(Theme::accentInk(), BorderW));
        p.setBrush(Qt::NoBrush);
        const QRectF ring = track.adjusted(-2.0, -2.0, 2.0, 2.0);
        p.drawRoundedRect(ring, ring.height() / 2.0, ring.height() / 2.0);
    }
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
    if (m_pressed) {
        m_pressed = false;
        m_squashAnim->stop();
        m_squashAnim->setStartValue(m_squash);
        m_squashAnim->setEndValue(0.0);
        m_squashAnim->start();
    }
    update();
    QWidget::leaveEvent(e);
}

void ToggleSwitch::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        m_squashAnim->stop();
        m_squashAnim->setStartValue(m_squash);
        m_squashAnim->setEndValue(1.0);
        m_squashAnim->start();
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

        m_squashAnim->stop();
        m_squashAnim->setStartValue(m_squash);
        m_squashAnim->setEndValue(0.0);
        m_squashAnim->start();

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
