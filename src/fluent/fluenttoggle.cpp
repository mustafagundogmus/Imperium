#include "fluenttoggle.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

namespace {
constexpr qreal KnobSize = 12.0;
constexpr qreal KnobLeftOff = 3.0;
constexpr qreal KnobLeftOn = 23.0;
constexpr int SlideMs = 150;

QColor lerp(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
} // namespace

FluentToggle::FluentToggle(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(sizeHint());
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

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, qOverload<>(&QWidget::update));
}

QSize FluentToggle::sizeHint() const
{
    return {LabelWidth + LabelGap + TrackWidth, TrackHeight};
}

void FluentToggle::setChecked(bool on, bool animate)
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

void FluentToggle::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // The word first, right-aligned in its column.
    const QString label = Locale::tr(m_checked ? QStringLiteral("fluent.on") : QStringLiteral("fluent.off"));
    Css::drawCentered(&p, QRectF(0, 0, LabelWidth, height()), Theme::sans(12), t.textSec, label,
                      Qt::AlignRight);

    const qreal fill = qBound(0.0, m_t, 1.0);
    const QRectF track(LabelWidth + LabelGap + 0.5, 0.5, TrackWidth - 1.0, TrackHeight - 1.0);

    QColor trackFill = t.accent;
    trackFill.setAlphaF(fill);
    QColor border = lerp(t.toggleOffBorder, t.accent, fill);
    if (hasFocus())
        border = t.accentText;
    else if (m_hovered && fill < 0.5)
        border = lerp(border, t.textSec, 0.35);

    p.setPen(QPen(border, 1.0));
    p.setBrush(trackFill);
    p.drawRoundedRect(track, TrackHeight / 2.0, TrackHeight / 2.0);

    const qreal knobX = track.left() - 0.5 + KnobLeftOff + (KnobLeftOn - KnobLeftOff) * fill;
    const qreal knobY = (TrackHeight - KnobSize) / 2.0;
    QColor knob = lerp(t.knobOff, t.onAccent, fill);
    if (m_pressed)
        knob = lerp(knob, t.text, 0.2);
    p.setPen(Qt::NoPen);
    p.setBrush(knob);
    p.drawEllipse(QRectF(knobX, knobY, KnobSize, KnobSize));
}

void FluentToggle::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void FluentToggle::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void FluentToggle::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void FluentToggle::mouseReleaseEvent(QMouseEvent *e)
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

void FluentToggle::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        setChecked(!m_checked);
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}
