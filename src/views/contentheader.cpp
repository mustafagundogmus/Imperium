#include "contentheader.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"
#include "../widgets/segmentedcontrol.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr qreal PadX = 18.0;
constexpr qreal PadTop = 14.0;
constexpr qreal PadBottom = 10.0;
constexpr qreal Gap = 12.0;        // between the left group, the filter and the glyph
constexpr qreal TextGap = 10.0;    // between title, subtitle and pending label
constexpr int SortSize = 13;
} // namespace

ContentHeader::ContentHeader(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);

    m_filter = new SegmentedControl({QStringLiteral("Tümü"),
                                     QStringLiteral("Etkin"),
                                     QStringLiteral("Değişen")},
                                    this);
    connect(m_filter, &SegmentedControl::currentIndexChanged, this, &ContentHeader::filterChanged);

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    setFixedHeight(sizeHint().height());
}

qreal ContentHeader::contentHeight() const
{
    // `align-items:center` — the row is as tall as its tallest child.
    const qreal group = Css::ascent(Theme::Font::pageTitle()) + Css::descent(Theme::Font::pageTitle());
    return m_controlsVisible ? qMax(group, qreal(m_filter->height())) : group;
}

QSize ContentHeader::sizeHint() const
{
    return {0, qRound(PadTop + contentHeight() + PadBottom)};
}

void ContentHeader::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    update();
}

void ContentHeader::setSubtitle(const QString &subtitle)
{
    if (m_subtitle == subtitle)
        return;
    m_subtitle = subtitle;
    update();
}

void ContentHeader::setPendingLabel(const QString &label)
{
    if (m_pending == label)
        return;
    m_pending = label;
    update();
}

void ContentHeader::setControlsVisible(bool visible)
{
    if (m_controlsVisible == visible)
        return;
    m_controlsVisible = visible;
    m_filter->setVisible(visible);
    setFixedHeight(sizeHint().height());
    updateGeometry();
    update();
}

void ContentHeader::setFilterIndex(int index)
{
    m_filter->setCurrentIndex(index);
}

QRectF ContentHeader::sortRect() const
{
    const qreal y = PadTop + (contentHeight() - SortSize) / 2.0;
    return {width() - PadX - SortSize, std::round(y), qreal(SortSize), qreal(SortSize)};
}

void ContentHeader::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    const qreal x = width() - PadX - SortSize - Gap - m_filter->width();
    const qreal y = PadTop + (contentHeight() - m_filter->height()) / 2.0;
    m_filter->move(qRound(x), qRound(y));
}

void ContentHeader::mouseMoveEvent(QMouseEvent *e)
{
    const bool hovered = m_controlsVisible && sortRect().contains(e->position());
    if (hovered != m_sortHovered) {
        m_sortHovered = hovered;
        setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void ContentHeader::leaveEvent(QEvent *e)
{
    if (m_sortHovered) {
        m_sortHovered = false;
        update();
    }
    QWidget::leaveEvent(e);
}

void ContentHeader::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_controlsVisible && sortRect().contains(e->position())) {
        m_sortActive = !m_sortActive;
        update();
        Q_EMIT sortToggled(m_sortActive);
    }
    QWidget::mouseReleaseEvent(e);
}

void ContentHeader::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont &titleFont = Font::pageTitle();
    const QFont &subFont = Font::pageSub();

    // All three strings share one baseline (`align-items:baseline`), and the group as a
    // whole is centred in the row.
    const qreal groupH = Css::ascent(titleFont) + Css::descent(titleFont);
    const qreal groupTop = PadTop + (contentHeight() - groupH) / 2.0;
    const qreal baseline = groupTop + Css::ascent(titleFont);

    qreal right = width() - PadX;
    if (m_controlsVisible)
        right = m_filter->x() - Gap;

    qreal x = PadX;
    const QRectF titleBox(x, 0, qMax(0.0, right - x), height());
    const qreal titleW = qMin(Css::textWidth(titleFont, m_title), titleBox.width());
    Css::drawText(&p, titleBox, baseline, titleFont, Color::TextPrimary(), m_title,
                  Qt::AlignLeft, true);

    x += titleW + TextGap;
    if (!m_subtitle.isEmpty() && x < right) {
        const QRectF box(x, 0, right - x, height());
        Css::drawText(&p, box, baseline, subFont, Color::TextDim(), m_subtitle, Qt::AlignLeft, true);
        x += qMin(Css::textWidth(subFont, m_subtitle), box.width());
    }

    if (!m_pending.isEmpty()) {
        // Same 10px flex gap as between the title and the subtitle.
        const qreal px = x + (m_subtitle.isEmpty() ? 0.0 : TextGap);
        if (px < right)
            Css::drawText(&p, QRectF(px, 0, right - px, height()), baseline, subFont,
                          Theme::accentInk(), m_pending, Qt::AlignLeft, true);
    }

    if (m_controlsVisible) {
        const QColor c = m_sortActive ? Theme::accentInk()
                         : m_sortHovered ? Color::TextMono()
                                         : Color::TextMuted();
        p.drawPixmap(sortRect().topLeft(), Icons::sort(c, devicePixelRatioF()));
    }
}
