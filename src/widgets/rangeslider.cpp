#include "rangeslider.h"

#include "../css.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {

constexpr qreal Track = 150.0;    // the rail itself
constexpr qreal TrackH = 3.0;
constexpr qreal Knob = 11.0;
constexpr qreal LabelGap = 10.0;
constexpr qreal LabelW = 58.0;    // room for "400 ms" without the rail moving as it changes

} // namespace

RangeSlider::RangeSlider(const QStringList &labels, QWidget *parent)
    : QWidget(parent)
    , m_labels(labels)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedSize(sizeHint());
        update();
    });
}

QSize RangeSlider::sizeHint() const
{
    const qreal h = qMax(Knob, Css::normalLine(Theme::Font::infoValueMono()));
    return {qRound(Track + LabelGap + LabelW), qRound(h)};
}

void RangeSlider::setCurrentIndex(int index)
{
    const int clamped = qBound(0, index, int(m_labels.size()) - 1);
    if (clamped == m_current)
        return;
    m_current = clamped;
    update();
}

qreal RangeSlider::trackWidth() const
{
    return Track - Knob;   // the knob's centre travels this far
}

qreal RangeSlider::knobX(int index) const
{
    if (m_labels.size() < 2)
        return Knob / 2.0;
    return Knob / 2.0 + trackWidth() * index / (m_labels.size() - 1);
}

int RangeSlider::indexAt(qreal x) const
{
    if (m_labels.size() < 2)
        return 0;
    const qreal t = (x - Knob / 2.0) / trackWidth();
    return qBound(0, qRound(t * (m_labels.size() - 1)), int(m_labels.size()) - 1);
}

void RangeSlider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    const int hit = indexAt(e->position().x());
    if (hit != m_current) {
        m_current = hit;
        update();
        Q_EMIT currentIndexChanged(m_current);
    }
}

void RangeSlider::mouseMoveEvent(QMouseEvent *e)
{
    const bool inside = e->position().x() <= Track;
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

void RangeSlider::mouseReleaseEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    m_dragging = false;
}

void RangeSlider::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    if (m_hovered) {
        m_hovered = false;
        update();
    }
}

void RangeSlider::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal mid = height() / 2.0;
    const qreal x = knobX(m_current);

    // rail, then the travelled part of it in the accent
    p.setPen(Qt::NoPen);
    p.setBrush(Color::ToggleOff());
    p.drawRoundedRect(QRectF(0, mid - TrackH / 2.0, Track, TrackH), TrackH / 2.0, TrackH / 2.0);

    p.setBrush(accent());
    p.drawRoundedRect(QRectF(0, mid - TrackH / 2.0, x, TrackH), TrackH / 2.0, TrackH / 2.0);

    // The stops, so the discreteness is visible rather than a surprise on release.
    if (m_labels.size() > 1 && m_labels.size() <= 12) {
        p.setBrush(Color::TileBorder());
        for (int i = 0; i < m_labels.size(); ++i)
            p.drawEllipse(QPointF(knobX(i), mid), 1.0, 1.0);
    }

    p.setPen(QPen(m_hovered || m_dragging ? accent() : Color::ToggleOffBorder(), 1.0));
    p.setBrush(Color::KnobOff());
    p.drawEllipse(QPointF(x, mid), Knob / 2.0 - 0.5, Knob / 2.0 - 0.5);

    const QFont &font = Font::infoValueMono();
    const qreal line = Css::normalLine(font);
    Css::drawText(&p, QRectF(Track + LabelGap, 0, LabelW, height()),
                  Css::baseline(font, (height() - line) / 2.0, line), font,
                  Color::TextMono(), m_labels.value(m_current), Qt::AlignLeft);
}
