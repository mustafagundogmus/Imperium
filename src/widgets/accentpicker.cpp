#include "accentpicker.h"

#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int Dot = 14;
constexpr int Gap = 10;
constexpr qreal RingOffset = 3.0;
} // namespace

AccentPicker::AccentPicker(QWidget *parent)
    : QWidget(parent)
    , m_colours(Theme::accentPresets())
{
    setMouseTracking(true);
    setFixedSize(sizeHint());
    setCursor(Qt::PointingHandCursor);
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

QSize AccentPicker::sizeHint() const
{
    const int n = int(m_colours.size());
    // The ring needs room on both ends, hence the extra offset on each side.
    return {n * Dot + (n - 1) * Gap + int(2 * RingOffset), Dot + int(2 * RingOffset)};
}

int AccentPicker::indexAt(const QPointF &pos) const
{
    for (int i = 0; i < m_colours.size(); ++i) {
        const qreal x = RingOffset + i * (Dot + Gap);
        if (pos.x() >= x - Gap / 2.0 && pos.x() < x + Dot + Gap / 2.0)
            return i;
    }
    return -1;
}

void AccentPicker::mouseMoveEvent(QMouseEvent *e)
{
    const int hit = indexAt(e->position());
    if (hit != m_hovered) {
        m_hovered = hit;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void AccentPicker::leaveEvent(QEvent *e)
{
    if (m_hovered != -1) {
        m_hovered = -1;
        update();
    }
    QWidget::leaveEvent(e);
}

void AccentPicker::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void AccentPicker::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        const int hit = indexAt(e->position());
        if (hit >= 0)
            Q_EMIT picked(m_colours.at(hit));
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void AccentPicker::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor current = Theme::accent();

    for (int i = 0; i < m_colours.size(); ++i) {
        const QColor c = m_colours.at(i);
        const QRectF dot(RingOffset + i * (Dot + Gap), RingOffset, Dot, Dot);

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(dot);

        const bool selected = c.rgb() == current.rgb();
        if (selected || i == m_hovered) {
            QColor ring = c;
            if (!selected)
                ring.setAlpha(0x80);
            p.setPen(QPen(ring, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(dot.adjusted(-RingOffset + 0.5, -RingOffset + 0.5,
                                       RingOffset - 0.5, RingOffset - 0.5));
        }
    }
}
