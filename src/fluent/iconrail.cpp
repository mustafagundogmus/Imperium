#include "iconrail.h"
#include "fluenticons.h"
#include "../css.h"
#include "../theme.h"

#include <cmath>

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QToolTip>
#include <QVariantAnimation>

namespace {
constexpr qreal Button = 40.0;
constexpr qreal PadTop = 4.0;
constexpr qreal PadBottom = 8.0;
constexpr qreal Gap = 2.0;
constexpr qreal Radius = 5.0;
constexpr int IconSize = 18;
constexpr qreal Stroke = 1.75;
constexpr qreal BarW = 3.0;
constexpr qreal BarH = 16.0;
constexpr qreal BarTop = 12.0;
constexpr qreal BarRadius = 2.0;
constexpr qreal LabelGap = 10.0;     // icon box → name
constexpr qreal LabelSlide = 12.0;   // how far the name travels as the rail opens
constexpr qreal LabelPadRight = 8.0;
constexpr int OpenDelayMs = 140;
constexpr int SlideMs = 180;

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
} // namespace

IconRail::IconRail(QWidget *parent)
    : QWidget(parent)
{
    // No fixed width: the chrome places the rail and the rail widens itself — a fixed
    // width would clamp the resize the animation asks for.
    resize(Theme::Fluent::RailWidth, height());
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);

    m_slide = new QVariantAnimation(this);
    m_slide->setDuration(SlideMs);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slide, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { setProgress(v.toReal()); });

    m_openDelay = new QTimer(this);
    m_openDelay->setSingleShot(true);
    m_openDelay->setInterval(OpenDelayMs);
    connect(m_openDelay, &QTimer::timeout, this, [this] { slideTo(1.0); });

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, qOverload<>(&QWidget::update));
}

void IconRail::setEntries(const QVector<Entry> &entries, const Entry &settings)
{
    m_entries = entries;
    m_settings = settings;
    update();
}

void IconRail::setLabels(const QStringList &labels, const QString &settingsLabel)
{
    for (int i = 0; i < m_entries.size() && i < labels.size(); ++i)
        m_entries[i].label = labels.at(i);
    m_settings.label = settingsLabel;
    update();
}

void IconRail::setSelected(int index)
{
    if (m_selected == index)
        return;
    m_selected = index;
    update();
}

QRectF IconRail::rowRect(int index) const
{
    // The buttons keep the closed rail's 8px margin on both sides whatever the width, so
    // the icons stay put while the rail opens and only the rows lengthen.
    const qreal x = (Theme::Fluent::RailWidth - Button) / 2.0;
    const qreal w = width() - 2 * x;
    if (index < 0)
        return {x, height() - PadBottom - Button, w, Button};
    return {x, PadTop + index * (Button + Gap), w, Button};
}

int IconRail::indexAt(const QPointF &pos) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (rowRect(i).contains(pos))
            return i;
    if (rowRect(-1).contains(pos))
        return -1;
    return -2;
}

void IconRail::slideTo(qreal progress)
{
    m_slide->stop();
    if (qFuzzyCompare(1.0 + progress, 1.0 + m_progress))
        return;
    m_slide->setStartValue(m_progress);
    m_slide->setEndValue(progress);
    m_slide->start();
}

void IconRail::setProgress(qreal progress)
{
    m_progress = qBound(0.0, progress, 1.0);
    const int w = qRound(Theme::Fluent::RailWidth
                         + (ExpandedWidth - Theme::Fluent::RailWidth) * m_progress);
    if (w != width())
        resize(w, height());
    update();
}

void IconRail::enterEvent(QEnterEvent *e)
{
    m_openDelay->start();
    QWidget::enterEvent(e);
}

void IconRail::leaveEvent(QEvent *e)
{
    m_openDelay->stop();
    if (m_hovered != -2) {
        m_hovered = -2;
        update();
    }
    m_pressed = -2;
    slideTo(0.0);
    QWidget::leaveEvent(e);
}

void IconRail::mouseMoveEvent(QMouseEvent *e)
{
    const int hit = indexAt(e->position());
    if (hit != m_hovered) {
        m_hovered = hit;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void IconRail::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = indexAt(e->position());
        e->accept();   // never a window drag, whatever was hit
        return;
    }
    QWidget::mousePressEvent(e);
}

void IconRail::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const int hit = indexAt(e->position());
        const int pressed = m_pressed;
        m_pressed = -2;
        e->accept();
        if (hit != -2 && hit == pressed)
            Q_EMIT activated(hit);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

bool IconRail::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        // The open rail shows the names itself; the tooltip is for the closed one.
        auto *help = static_cast<QHelpEvent *>(e);
        const int hit = indexAt(help->pos());
        if (hit == -2 || m_progress > 0.5)
            QToolTip::hideText();
        else
            QToolTip::showText(help->globalPos(), hit < 0 ? m_settings.label : m_entries.at(hit).label, this);
        return true;
    }
    return QWidget::event(e);
}

void IconRail::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Closed, the rail is transparent on the card's mica like the handoff has it. Open,
    // it lies over the pane and has to be opaque: the mica, lifted a step towards the
    // surface as it opens, with a hairline at the edge it covers the pane with.
    if (m_progress > 0.0) {
        p.fillRect(rect(), mix(t.mica, t.surface, m_progress));
        QColor edge = t.divider;
        edge.setAlphaF(edge.alphaF() * m_progress);
        Css::hairline(&p, QRectF(width() - 1.0, 0, 1.0, height()), edge);
    }

    const QFont labelFont = Theme::sans(13);
    const QFont labelFontSelected = Theme::sans(13, Theme::Weight::Medium);

    const auto draw = [&](int index, const Entry &entry) {
        const QRectF r = rowRect(index);
        const bool selected = index == m_selected;
        if (selected || index == m_hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(selected ? t.selected : t.subtleHover);
            p.drawRoundedRect(r, Radius, Radius);
        }
        if (selected) {
            p.setBrush(t.accent);
            p.drawRoundedRect(QRectF(r.left(), r.top() + BarTop, BarW, BarH), BarRadius, BarRadius);
        }
        if (entry.glyph) {
            const QPixmap glyph = FluentIcons::draw(*entry.glyph,
                                                    selected ? t.text : Theme::Color::IconStroke(),
                                                    IconSize, Stroke, devicePixelRatioF());
            p.drawPixmap(QPointF(r.left() + std::round((Button - IconSize) / 2.0),
                                 r.top() + std::round((Button - IconSize) / 2.0)),
                         glyph);
        }

        // The name, fading and sliding in with the width, kept inside the row so it does
        // not show past the edge before the row is long enough for it.
        if (m_progress <= 0.0 || entry.label.isEmpty())
            return;
        const qreal x = r.left() + Button + LabelGap - LabelSlide * (1.0 - m_progress);
        const QRectF box(x, r.top(), r.right() - LabelPadRight - x, r.height());
        if (box.width() <= 0)
            return;
        p.save();
        p.setClipRect(r);
        p.setOpacity(m_progress);
        Css::drawCentered(&p, box, selected ? labelFontSelected : labelFont,
                          selected ? t.text : t.textSec, entry.label, Qt::AlignLeft, /*elide=*/true);
        p.restore();
    };

    for (int i = 0; i < m_entries.size(); ++i)
        draw(i, m_entries.at(i));
    draw(-1, m_settings);
}
