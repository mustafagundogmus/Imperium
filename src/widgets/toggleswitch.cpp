#include "toggleswitch.h"
#include "../theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

namespace {

QColor lerp(const QColor &a, const QColor &b, qreal t)
{
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

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(150);                       // .15s
    m_anim->setEasingCurve(QEasingCurve::OutCubic); // CSS `ease`
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_t = v.toReal();
        update();
    });

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
}

QSize ToggleSwitch::sizeHint() const
{
    return {Theme::Metric::ToggleWidth, Theme::Metric::ToggleHeight};
}

void ToggleSwitch::setChecked(bool on, bool animate)
{
    if (m_checked == on)
        return;
    m_checked = on;

    const qreal target = on ? 1.0 : 0.0;
    m_anim->stop();
    if (animate) {
        m_anim->setStartValue(m_t);
        m_anim->setEndValue(target);
        m_anim->start();
    } else {
        m_t = target;
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
    const QColor bg     = lerp(Color::ToggleOff(), accent, m_t);
    const QColor border = lerp(Color::ToggleOffBorder(), accent, m_t);

    // border-box 26×15 with a 1px border → stroke on the half-pixel grid.
    const QRectF track(0.5, 0.5, width() - 1.0, height() - 1.0);
    p.setPen(QPen(border, 1.0));
    p.setBrush(bg);
    p.drawRoundedRect(track, Metric::ToggleRadius, Metric::ToggleRadius);

    // Knob: `position:absolute; top:2px; left:2px|13px` measured from the padding box,
    // i.e. inset by the 1px border → centre travels from x=7.5 to x=20.5.
    const qreal r = Metric::KnobDiameter / 2.0;
    const qreal x0 = 1.0 + Metric::KnobInset + r;
    const qreal cx = x0 + Metric::KnobTravel * m_t;
    const qreal cy = 1.0 + Metric::KnobInset + r;

    p.setPen(Qt::NoPen);
    p.setBrush(m_checked ? Color::KnobOn() : Color::KnobOff());
    p.drawEllipse(QPointF(cx, cy), r, r);

    if (hasFocus()) {
        p.setPen(QPen(Theme::accentInk(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(track.adjusted(-1.5, -1.5, 1.5, 1.5),
                          Metric::ToggleRadius + 1.5, Metric::ToggleRadius + 1.5);
    }
}

void ToggleSwitch::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_pressed) {
        m_pressed = false;
        if (rect().contains(e->pos()))
            setChecked(!m_checked);
        e->accept();
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
