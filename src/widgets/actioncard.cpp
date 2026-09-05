#include "actioncard.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

QPixmap renderCardIcon(const QString &d, const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral("<path d=\"%1\" fill=\"%2\"/>").arg(d, hex(c));
    return Icons::fragment(QStringLiteral("cardicon:%1:%2").arg(d, hex(c)), inner, 24.0, QSize(22, 22), dpr);
}

} // namespace

ActionCard::ActionCard(const QString &title, const QString &desc, const QString &svgPath,
                       QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_desc(desc)
    , m_svgPath(svgPath)
{
    setFixedHeight(76);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
}

void ActionCard::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    update();
}

void ActionCard::setDesc(const QString &desc)
{
    if (m_desc == desc)
        return;
    m_desc = desc;
    update();
}

void ActionCard::setIconPath(const QString &svgPath)
{
    if (m_svgPath == svgPath)
        return;
    m_svgPath = svgPath;
    update();
}

QSize ActionCard::sizeHint() const
{
    return {0, 76};
}

void ActionCard::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void ActionCard::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void ActionCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void ActionCard::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const bool inside = rect().contains(e->pos());
        m_pressed = false;
        update();
        if (inside)
            Q_EMIT clicked();
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void ActionCard::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal rad = Metric::ControlRadius + 2.0;

    // 1. Arka plan ve kenarlık çizimi
    QColor bg = Color::Tile();
    QColor border = Color::TileBorder();

    if (m_pressed) {
        bg = Theme::accentSoft();
        border = Theme::accent();
    } else if (m_hovered) {
        bg = Color::SurfaceHover();
        border = Theme::accent();
    }

    p.setPen(border);
    p.setBrush(bg);
    p.drawRoundedRect(r, rad, rad);

    // 2. Sol ikon rozeti (Badge)
    const qreal badgeSize = 44.0;
    const qreal badgeX = 16.0;
    const qreal badgeY = (height() - badgeSize) / 2.0;
    const QRectF badgeRect(badgeX, badgeY, badgeSize, badgeSize);

    QColor badgeBg = isLightFamily(Theme::appearance()) ? Theme::accentSoft() : Color::Surface();
    if (m_hovered)
        badgeBg = Theme::accentSoft();

    p.setPen(Qt::NoPen);
    p.setBrush(badgeBg);
    p.drawRoundedRect(badgeRect, Metric::ControlRadius, Metric::ControlRadius);

    // İkon çizimi
    if (!m_svgPath.isEmpty()) {
        const qreal dpr = devicePixelRatioF();
        const QPixmap iconPm = renderCardIcon(m_svgPath, Theme::accent(), dpr);
        const qreal iconX = badgeX + (badgeSize - 22.0) / 2.0;
        const qreal iconY = badgeY + (badgeSize - 22.0) / 2.0;
        p.drawPixmap(QPointF(iconX, iconY), iconPm);
    }

    // 3. Başlık ve Açıklama Metinleri
    const qreal textX = badgeX + badgeSize + 16.0;
    const qreal arrowSpace = 40.0;
    const qreal textW = width() - textX - arrowSpace;

    if (textW > 0) {
        // Başlık
        const QFont titleFont = Theme::sans(13.5, Weight::SemiBold);
        const QColor titleColor = m_hovered ? Theme::accentInk() : Color::TextPrimary();
        p.setFont(titleFont);
        p.setPen(titleColor);

        const QRectF titleBox(textX, 18.0, textW, 20.0);
        p.drawText(titleBox, Qt::AlignLeft | Qt::AlignVCenter, m_title);

        // Açıklama
        const QFont descFont = Font::tweakDesc();
        p.setFont(descFont);
        p.setPen(Color::TextSecondary());

        const QRectF descBox(textX, 40.0, textW, 20.0);
        p.drawText(descBox, Qt::AlignLeft | Qt::AlignVCenter, m_desc);
    }

    // 4. Sağ Gezinme Oku (Chevron / Arrow →)
    const qreal arrowOffset = m_hovered ? 2.0 : 0.0;
    const QRectF arrowBox(width() - 32.0 + arrowOffset, (height() - 20.0) / 2.0, 20.0, 20.0);
    p.setFont(Theme::sans(14.0, Weight::SemiBold));
    p.setPen(m_hovered ? Theme::accent() : Color::TextFaint());
    p.drawText(arrowBox, Qt::AlignCenter, QStringLiteral("→"));
}
