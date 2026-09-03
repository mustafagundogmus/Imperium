#include "fluentslider.h"
#include "../css.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr qreal Total = 220.0;
constexpr qreal LabelW = 44.0;
constexpr qreal LabelGap = 12.0;
constexpr qreal Knob = 20.0;
constexpr qreal Dot = 10.0;
constexpr qreal RailH = 4.0;
constexpr qreal Height = 20.0;
} // namespace

FluentSlider::FluentSlider(const QStringList &labels, QWidget *parent)
    : QWidget(parent)
    , m_labels(labels)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, qOverload<>(&QWidget::update));
}

QSize FluentSlider::sizeHint() const
{
    return {qRound(Total), qRound(Height)};
}

void FluentSlider::setCurrentIndex(int index)
{
    const int clamped = qBound(0, index, int(m_labels.size()) - 1);
    if (clamped == m_current)
        return;
    m_current = clamped;
    update();
}

qreal FluentSlider::railLeft() const
{
    // The knob overhangs the rail's ends by its radius in the design (`left: calc(pct -
    // 10px)`); giving the rail that much inset keeps the knob inside the widget instead.
    return Knob / 2.0;
}

qreal FluentSlider::railWidth() const
{
    return Total - LabelGap - LabelW - Knob;
}

qreal FluentSlider::knobX(int index) const
{
    if (m_labels.size() < 2)
        return railLeft();
    return railLeft() + railWidth() * index / (m_labels.size() - 1);
}

int FluentSlider::indexAt(qreal x) const
{
    if (m_labels.size() < 2)
        return 0;
    const qreal t = (x - railLeft()) / railWidth();
    return qBound(0, qRound(t * (m_labels.size() - 1)), int(m_labels.size()) - 1);
}

void FluentSlider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    if (e->position().x() > railLeft() + railWidth() + Knob / 2.0) {
        e->accept();   // the value label: not the rail, and not a window drag either
        return;
    }
    m_dragging = true;
    const int hit = indexAt(e->position().x());
    if (hit != m_current) {
        m_current = hit;
        update();
        Q_EMIT currentIndexChanged(m_current);
    }
    e->accept();
}

void FluentSlider::mouseMoveEvent(QMouseEvent *e)
{
    const bool inside = e->position().x() <= railLeft() + railWidth() + Knob / 2.0;
    if (inside != m_hovered) {
        m_hovered = inside;
        update();
    }
    if (!m_dragging)
        return;
    const int hit = indexAt(e->position().x());
    if (hit != m_current) {
        m_current = hit;
        update();
        Q_EMIT currentIndexChanged(m_current);
    }
}

void FluentSlider::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

void FluentSlider::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    if (m_hovered) {
        m_hovered = false;
        update();
    }
}

void FluentSlider::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal mid = height() / 2.0;
    const qreal x = knobX(m_current);
    const qreal left = railLeft();
    const qreal right = left + railWidth();

    p.setPen(Qt::NoPen);
    p.setBrush(t.track);
    p.drawRoundedRect(QRectF(left, mid - RailH / 2.0, right - left, RailH), RailH / 2.0, RailH / 2.0);
    p.setBrush(t.accent);
    p.drawRoundedRect(QRectF(left, mid - RailH / 2.0, x - left, RailH), RailH / 2.0, RailH / 2.0);

    // The shadow under the knob: `0 1px 3px rgba(0,0,0,.2)`, drawn as two soft rings.
    p.setBrush(QColor(0, 0, 0, 18));
    p.drawEllipse(QPointF(x, mid + 1.5), Knob / 2.0 + 1.5, Knob / 2.0 + 1.5);
    p.setBrush(QColor(0, 0, 0, 26));
    p.drawEllipse(QPointF(x, mid + 1.0), Knob / 2.0 + 0.5, Knob / 2.0 + 0.5);

    p.setPen(QPen(m_hovered || m_dragging ? t.accent : t.controlBorder, 1.0));
    p.setBrush(t.card);
    p.drawEllipse(QPointF(x, mid), Knob / 2.0 - 0.5, Knob / 2.0 - 0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(t.accent);
    p.drawEllipse(QPointF(x, mid), Dot / 2.0, Dot / 2.0);

    Css::drawCentered(&p, QRectF(Total - LabelW, 0, LabelW, height()), Theme::mono(12), t.text,
                      m_labels.value(m_current), Qt::AlignRight);
}
