#include "categoryrow.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr qreal PadX = 10.0;
constexpr qreal Gap = 9.0;
constexpr int IconSize = 12;
} // namespace

CategoryRow::CategoryRow(const QString &id, const QString &name, const QString &iconPath,
                         const QString &count, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
    , m_name(name)
    , m_iconPath(iconPath)
    , m_count(count)
{
    setFixedHeight(Theme::Metric::CategoryHeight);
    setCursor(Qt::ArrowCursor);   // the mockup keeps `cursor:default` on these rows
    setAttribute(Qt::WA_Hover, true);
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

QSize CategoryRow::sizeHint() const
{
    return {0, Theme::Metric::CategoryHeight};
}

void CategoryRow::setSelected(bool on)
{
    if (m_selected == on)
        return;
    m_selected = on;
    update();
}

void CategoryRow::setName(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    updateGeometry();
    update();
}

void CategoryRow::setCount(const QString &count)
{
    if (m_count == count)
        return;
    m_count = count;
    update();
}

void CategoryRow::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void CategoryRow::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void CategoryRow::mousePressEvent(QMouseEvent *e)
{
    // Claim the press so it cannot bubble up to the window's resize hit-testing.
    if (e->button() == Qt::LeftButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void CategoryRow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (rect().contains(e->pos()))
            Q_EMIT activated(m_id);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void CategoryRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = rect();

    // Selected wins over hover: the accent wash stays put while the pointer is over it.
    if (m_selected) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::accentSoft());
        p.drawRoundedRect(r, Metric::ControlRadius, Metric::ControlRadius);
    } else if (m_hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(Color::SurfaceHover());
        p.drawRoundedRect(r, Metric::ControlRadius, Metric::ControlRadius);
    }

    const QColor fg = m_selected ? Theme::accentInk() : Color::TextSecondary();

    const qreal dpr = devicePixelRatioF();
    const QPixmap icon = Icons::category(m_iconPath, fg, dpr);
    const qreal iconY = std::round((height() - IconSize) / 2.0);
    p.drawPixmap(QPointF(PadX, iconY), icon);

    const QFont &nameFont = m_selected ? Font::categoryNameSelected() : Font::categoryName();

    qreal countW = 0;
    if (!m_count.isEmpty()) {
        const QFont &countFont = Font::categoryCount();
        countW = Css::textWidth(countFont, m_count);
        const QRectF countBox(0, 0, width() - PadX, height());
        Css::drawCentered(&p, countBox, countFont, Color::TextFaint(), m_count, Qt::AlignRight);
    }

    const qreal nameX = PadX + IconSize + Gap;
    const qreal nameW = width() - nameX - PadX - (countW > 0 ? countW + Gap : 0.0);
    if (nameW > 0)
        Css::drawCentered(&p, QRectF(nameX, 0, nameW, height()), nameFont, fg, m_name,
                          Qt::AlignLeft, /*elide=*/true);
}
