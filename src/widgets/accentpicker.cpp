#include "accentpicker.h"

#include "../theme.h"
#include "segmentedcontrol.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

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
    // The height is the pill height, which comes out of the font — so the same signal the
    // chip grids re-measure on has to re-measure this one.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedSize(sizeHint());
        updateGeometry();
        update();
    });
}

QSize AccentPicker::sizeHint() const
{
    const int n = int(m_colours.size());
    // The ring needs room on both ends, hence the extra offset on each side. The height is
    // the app's pill height rather than the dot's own: it puts the strip on the same
    // rhythm as the controls stacked with it and gives the 14px dots a click band that
    // grows with the interface scale.
    return {n * Dot + (n - 1) * Gap + int(2 * RingOffset),
            qMax(Dot + int(2 * RingOffset), qCeil(SegmentedControl::controlHeight()))};
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

    // The strip is as tall as a pill and the dot is not, so the row of swatches sits in
    // the middle of it rather than at the top.
    const qreal top = (height() - Dot) / 2.0;

    for (int i = 0; i < m_colours.size(); ++i) {
        const QColor c = m_colours.at(i);
        const QRectF dot(RingOffset + i * (Dot + Gap), top, Dot, Dot);

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
