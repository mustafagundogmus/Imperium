#include "segmentedcontrol.h"
#include "../css.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
constexpr qreal PadX = 10.0;
constexpr qreal PadY = 3.0;
} // namespace

SegmentedControl::SegmentedControl(const QStringList &labels, QWidget *parent)
    : QWidget(parent)
    , m_labels(labels)
{
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    const qreal h = 2 /*border*/ + 2 * PadY + Css::normalLine(Theme::Font::segment());
    setFixedHeight(qRound(h));
    setFixedWidth(sizeHint().width());
}

qreal SegmentedControl::segmentWidth(int index) const
{
    return 2 * PadX + Css::textWidth(Theme::Font::segment(), m_labels.value(index));
}

QSize SegmentedControl::sizeHint() const
{
    qreal w = 2;   // left + right border
    for (int i = 0; i < m_labels.size(); ++i)
        w += segmentWidth(i) + (i > 0 ? 1.0 : 0.0);   // 1px rule between segments
    return {qCeil(w), height()};
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_labels.size() || index == m_current)
        return;
    m_current = index;
    update();
    Q_EMIT currentIndexChanged(index);
}

int SegmentedControl::segmentAt(const QPointF &pos) const
{
    qreal x = 1.0;
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal w = segmentWidth(i);
        if (pos.x() >= x && pos.x() < x + w)
            return i;
        x += w + 1.0;
    }
    return -1;
}

void SegmentedControl::mouseMoveEvent(QMouseEvent *e)
{
    const int hit = segmentAt(e->position());
    if (hit != m_hovered) {
        m_hovered = hit;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void SegmentedControl::leaveEvent(QEvent *e)
{
    if (m_hovered != -1) {
        m_hovered = -1;
        update();
    }
    QWidget::leaveEvent(e);
}

void SegmentedControl::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void SegmentedControl::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        const int hit = segmentAt(e->position());
        if (hit >= 0)
            setCurrentIndex(hit);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void SegmentedControl::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);

    // `overflow:hidden` — the active segment's fill is clipped by the rounded frame.
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, width(), height()),
                        Metric::ControlRadius, Metric::ControlRadius);

    const QFont &font = Font::segment();

    qreal x = 1.0;
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal w = segmentWidth(i);
        const QRectF seg(x, 1.0, w, height() - 2.0);

        if (i == m_current) {
            p.save();
            p.setClipPath(clip);
            p.fillRect(seg, Color::SurfaceActive());
            p.restore();
        }

        QColor fg = Color::TextMuted();
        if (i == m_current)
            fg = Color::TextPrimary();
        else if (i == m_hovered)
            fg = Color::TextMono();

        Css::drawCentered(&p, seg, font, fg, m_labels.at(i), Qt::AlignHCenter);

        x += w;
        if (i + 1 < m_labels.size()) {
            Css::hairline(&p, QRectF(x, 1.0, 1.0, height() - 2.0), Color::BorderControl());
            x += 1.0;
        }
    }

    p.setPen(QPen(Color::BorderControl(), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(frame, Metric::ControlRadius, Metric::ControlRadius);
}
