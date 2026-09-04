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
constexpr qreal LineGap = 4.0;   // between the lines of a wrapped control
} // namespace

SegmentedControl::SegmentedControl(const QStringList &labels, QWidget *parent)
    : QWidget(parent)
    , m_labels(labels)
{
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    // Every metric here comes out of the font, so the face changing means measuring again.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] { relayout(); });
    relayout();
}

qreal SegmentedControl::controlHeight()
{
    return 2 /*border*/ + 2 * PadY + Css::normalLine(Theme::Font::segment());
}

qreal SegmentedControl::segmentWidth(int index, qreal available) const
{
    const qreal natural = 2 * PadX + Css::textWidth(Theme::Font::segment(), m_labels.value(index));
    // A segment wider than the whole control may be is drawn at that width, elided.
    return available > 0.0 ? qMin(natural, available - 2.0) : natural;
}

QVector<SegmentedControl::Line> SegmentedControl::lines(qreal available) const
{
    QVector<Line> out;
    Line line;
    line.width = 2.0;   // left + right border
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal w = segmentWidth(i, available);
        qreal added = line.count > 0 ? 1.0 + w : w;   // the 1px rule before a segment that follows one
        if (line.count > 0 && available > 0.0 && line.width + added > available) {
            out.append(line);
            line = Line();
            line.first = i;
            line.width = 2.0;
            added = w;
        }
        line.width += added;
        ++line.count;
    }
    out.append(line);
    return out;
}

QSize SegmentedControl::sizeHint() const
{
    return {qCeil(lines(0.0).first().width), qRound(controlHeight())};
}

int SegmentedControl::heightForWidth(int width) const
{
    const int n = lines(width).size();
    return qRound(n * controlHeight() + (n - 1) * LineGap);
}

void SegmentedControl::setAvailableWidth(qreal width)
{
    if (qFuzzyCompare(1.0 + width, 1.0 + m_available))
        return;
    m_available = width;
    relayout();
}

void SegmentedControl::relayout()
{
    qreal w = 0.0;
    for (const Line &line : lines(m_available))
        w = qMax(w, line.width);
    setFixedSize(qCeil(w), heightForWidth(qFloor(m_available)));
    updateGeometry();
    update();
}

void SegmentedControl::setLabels(const QStringList &labels)
{
    if (labels.size() != m_labels.size())
        return;   // a language swap renames segments, it does not add or remove them
    m_labels = labels;
    relayout();
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_labels.size() || index == m_current)
        return;
    m_current = index;
    update();
    Q_EMIT currentIndexChanged(index);
}

QRectF SegmentedControl::segmentRect(int index) const
{
    const qreal h = controlHeight();
    qreal y = 0.0;
    for (const Line &line : lines(m_available)) {
        if (index >= line.first && index < line.first + line.count) {
            qreal x = 1.0;
            for (int i = line.first; i < index; ++i)
                x += segmentWidth(i, m_available) + 1.0;
            return QRectF(x, y + 1.0, segmentWidth(index, m_available), h - 2.0);
        }
        y += h + LineGap;
    }
    return QRectF();
}

int SegmentedControl::segmentAt(const QPointF &pos) const
{
    for (int i = 0; i < m_labels.size(); ++i)
        if (segmentRect(i).contains(pos))
            return i;
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

    const QFont &font = Font::segment();
    const qreal h = controlHeight();

    // Each line is its own group: frame, rules, and the active segment's fill clipped by
    // the rounded frame (`overflow:hidden`).
    qreal y = 0.0;
    for (const Line &line : lines(m_available)) {
        QPainterPath clip;
        clip.addRoundedRect(QRectF(0, y, line.width, h), Metric::ControlRadius, Metric::ControlRadius);

        qreal x = 1.0;
        for (int i = line.first; i < line.first + line.count; ++i) {
            const qreal w = segmentWidth(i, m_available);
            const QRectF seg(x, y + 1.0, w, h - 2.0);

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

            // Centred in the segment's padding box, and elided only when the segment was
            // capped. The box is the label's own width otherwise, and asking for an elision
            // there would hand a rounding error a chance to turn "Etkin" into "Etki…".
            const bool capped = m_available > 0.0 && segmentWidth(i, 0.0) > w;
            Css::drawCentered(&p, seg.adjusted(PadX, 0, -PadX, 0), font, fg, m_labels.at(i),
                              Qt::AlignHCenter, capped);

            x += w;
            if (i + 1 < line.first + line.count) {
                Css::hairline(&p, QRectF(x, y + 1.0, 1.0, h - 2.0), Color::BorderControl());
                x += 1.0;
            }
        }

        p.setPen(QPen(Color::BorderControl(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(0.5, y + 0.5, line.width - 1.0, h - 1.0),
                          Metric::ControlRadius, Metric::ControlRadius);
        y += h + LineGap;
    }
}
