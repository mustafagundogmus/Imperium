#include "cleanerrow.h"

#include "../css.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr qreal RowHeight = 48.0;
constexpr qreal PadX = 10.0;
constexpr qreal CheckSize = 15.0;
constexpr qreal CheckGap = 12.0;
constexpr qreal SizeGap = 14.0;
constexpr qreal TextGap = 1.0;

} // namespace

CleanerRow::CleanerRow(const QString &id, const QString &name, const QString &desc, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
    , m_name(name)
    , m_desc(desc)
{
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::ArrowCursor);
    setFixedHeight(qRound(RowHeight));

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, qOverload<>(&QWidget::update));
}

QSize CleanerRow::sizeHint() const
{
    return {0, qRound(RowHeight)};
}

void CleanerRow::setChecked(bool on)
{
    if (m_checked == on)
        return;
    m_checked = on;
    update();
}

void CleanerRow::setSize(const QString &text)
{
    m_size = text;
    update();
}

void CleanerRow::setStatus(const QString &text)
{
    m_status = text;
    update();
}

void CleanerRow::setDescription(const QString &text)
{
    m_desc = text;
    update();
}

void CleanerRow::setBusy(bool busy)
{
    m_busy = busy;
    update();
}

QRectF CleanerRow::checkboxRect() const
{
    return {PadX, (height() - CheckSize) / 2.0, CheckSize, CheckSize};
}

void CleanerRow::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void CleanerRow::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void CleanerRow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void CleanerRow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (!m_busy && rect().contains(e->pos())) {
            m_checked = !m_checked;
            update();
            Q_EMIT toggled(m_id, m_checked);
        }
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void CleanerRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    if (m_busy)
        p.setOpacity(0.55);

    const QRectF r = rect();
    if (m_checked) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::accentSoft());
        p.drawRoundedRect(r.adjusted(0, 1, 0, -1), Metric::ControlRadius, Metric::ControlRadius);
    } else if (m_hovered && !m_busy) {
        p.setPen(Qt::NoPen);
        p.setBrush(Color::SurfaceHover());
        p.drawRoundedRect(r.adjusted(0, 1, 0, -1), Metric::ControlRadius, Metric::ControlRadius);
    }

    // Checkbox — the same drawing DebloatRow uses, so the two lists read as one family.
    const QRectF box = checkboxRect();
    if (m_checked) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::accent());
        p.drawRoundedRect(box, Metric::BadgeRadius, Metric::BadgeRadius);
        QPainterPath check;
        check.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.54);
        check.lineTo(box.left() + box.width() * 0.42, box.top() + box.height() * 0.72);
        check.lineTo(box.left() + box.width() * 0.78, box.top() + box.height() * 0.30);
        QPen pen(QColor(0x1A, 0x1A, 0x1E));
        pen.setWidthF(1.6);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(check);
    } else {
        p.setPen(QPen(Color::BorderControl(), 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), Metric::BadgeRadius, Metric::BadgeRadius);
    }

    // The size, right-aligned in the figure face; the text block stops short of it.
    const QFont &sizeFont = Font::infoValueMono();
    const qreal sizeW = m_size.isEmpty() ? 0.0 : Css::textWidth(sizeFont, m_size);
    if (!m_size.isEmpty()) {
        const QRectF sizeBox(width() - PadX - sizeW, 0, sizeW, height());
        Css::drawText(&p, sizeBox, Css::centeredBaseline(sizeFont, sizeBox), sizeFont,
                      m_checked ? Color::TextPrimary() : Color::TextSecondary(), m_size,
                      Qt::AlignRight, false);
    }

    const qreal textX = box.right() + CheckGap;
    const qreal textRight = width() - PadX - sizeW - (sizeW > 0 ? SizeGap : 0.0);
    const qreal textW = textRight - textX;
    if (textW > 0) {
        const QFont &nameFont = Font::tweakName();
        const QFont &descFont = Font::tweakDesc();
        const qreal nameLine = Css::normalLine(nameFont);
        const qreal descLine = Css::line(descFont, 1.45);
        const qreal block = nameLine + TextGap + descLine;
        const qreal top = (height() - block) / 2.0;

        const QRectF textBox(textX, 0, textW, height());
        Css::drawText(&p, textBox, Css::baseline(nameFont, top, nameLine), nameFont,
                      Color::TextPrimary(), m_name, Qt::AlignLeft, true);

        const QString &desc = m_status.isEmpty() ? m_desc : m_status;
        const QColor descColor = m_status.isEmpty() ? Color::TextDesc() : Theme::accentInk();
        Css::drawText(&p, textBox, Css::baseline(descFont, top + nameLine + TextGap, descLine),
                      descFont, descColor, desc, Qt::AlignLeft, true);
    }
}
