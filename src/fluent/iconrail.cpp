#include "iconrail.h"
#include "fluenticons.h"
#include "../theme.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

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
} // namespace

IconRail::IconRail(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(Theme::Fluent::RailWidth);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
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
}

void IconRail::setSelected(int index)
{
    if (m_selected == index)
        return;
    m_selected = index;
    update();
}

QRectF IconRail::buttonRect(int index) const
{
    const qreal x = (width() - Button) / 2.0;
    if (index < 0)
        return {x, height() - PadBottom - Button, Button, Button};
    return {x, PadTop + index * (Button + Gap), Button, Button};
}

int IconRail::indexAt(const QPointF &pos) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (buttonRect(i).contains(pos))
            return i;
    if (buttonRect(-1).contains(pos))
        return -1;
    return -2;
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

void IconRail::leaveEvent(QEvent *e)
{
    if (m_hovered != -2) {
        m_hovered = -2;
        update();
    }
    m_pressed = -2;
    QWidget::leaveEvent(e);
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
        auto *help = static_cast<QHelpEvent *>(e);
        const int hit = indexAt(help->pos());
        if (hit == -2)
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

    const auto draw = [&](int index, const Entry &entry) {
        const QRectF r = buttonRect(index);
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
        if (!entry.glyph)
            return;
        const QPixmap glyph = FluentIcons::draw(*entry.glyph,
                                                selected ? t.text : Theme::Color::IconStroke(),
                                                IconSize, Stroke, devicePixelRatioF());
        p.drawPixmap(QPointF(r.left() + std::round((Button - IconSize) / 2.0),
                             r.top() + std::round((Button - IconSize) / 2.0)),
                     glyph);
    };

    for (int i = 0; i < m_entries.size(); ++i)
        draw(i, m_entries.at(i));
    draw(-1, m_settings);
}
