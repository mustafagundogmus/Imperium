#include "debloatrow.h"

#include "../css.h"
#include "../theme.h"
#include "buttons.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr qreal RowHeight = 56.0;
constexpr qreal PadX = 10.0;
constexpr qreal CheckSize = 15.0;
constexpr qreal CheckGap = 12.0;
constexpr qreal LogoSize = 30.0;
constexpr qreal LogoGap = 12.0;
constexpr qreal ButtonGap = 14.0;
constexpr qreal TextGap = 1.0;

} // namespace

DebloatRow::DebloatRow(const QString &id, const QPixmap &logo, const QString &name,
                       const QString &desc, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
    , m_logo(logo)
    , m_name(name)
    , m_desc(desc)
{
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::ArrowCursor);
    setFixedHeight(qRound(RowHeight));

    m_removeButton = new PillButton(PillButton::Ghost, QString(), this);

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, qOverload<>(&QWidget::update));
}

QSize DebloatRow::sizeHint() const
{
    return {0, qRound(RowHeight)};
}

void DebloatRow::setChecked(bool on)
{
    if (m_locked)
        on = false;
    if (m_checked == on)
        return;
    m_checked = on;
    update();
}

void DebloatRow::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    if (m_locked)
        m_checked = false;
    m_removeButton->setVisible(!m_locked);
    update();
}

void DebloatRow::setBusy(bool busy)
{
    m_removeButton->setEnabledLook(!busy);
}

void DebloatRow::setStatus(const QString &text)
{
    m_status = text;
    update();
}

void DebloatRow::clearStatus()
{
    if (m_status.isEmpty())
        return;
    m_status.clear();
    update();
}

QRectF DebloatRow::checkboxRect() const
{
    const qreal y = std::round((height() - CheckSize) / 2.0);
    return QRectF(PadX, y, CheckSize, CheckSize);
}

void DebloatRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    const QSize hint = m_removeButton->sizeHint();
    m_removeButton->resize(hint);
    m_removeButton->move(qRound(width() - PadX - hint.width()),
                         qRound((height() - hint.height()) / 2.0));
}

void DebloatRow::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void DebloatRow::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void DebloatRow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void DebloatRow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (!m_locked && rect().contains(e->pos())) {
            m_checked = !m_checked;
            update();
            Q_EMIT toggled(m_id, m_checked);
        }
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void DebloatRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF r = rect();
    if (m_checked) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::accentSoft());
        p.drawRoundedRect(r.adjusted(0, 1, 0, -1), Metric::ControlRadius, Metric::ControlRadius);
    } else if (m_hovered && !m_locked) {
        p.setPen(Qt::NoPen);
        p.setBrush(Color::SurfaceHover());
        p.drawRoundedRect(r.adjusted(0, 1, 0, -1), Metric::ControlRadius, Metric::ControlRadius);
    }

    // Checkbox — or a padlock where there is no choice to make.
    const QRectF box = checkboxRect();
    if (m_locked) {
        QPen pen(Color::TextFaint(), 1.2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Body, then the shackle arcing out of its top edge.
        const QRectF body(box.left() + 1.5, box.center().y() - 0.5, box.width() - 3.0,
                          box.height() / 2.0);
        p.drawRoundedRect(body, 1.5, 1.5);
        const qreal sw = body.width() * 0.52;
        QRectF shackle(body.center().x() - sw / 2.0, body.top() - sw * 0.72, sw, sw);
        p.drawArc(shackle, 0, 180 * 16);
    } else if (m_checked) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::accent());
        p.drawRoundedRect(box, Metric::BadgeRadius, Metric::BadgeRadius);
        QPainterPath check;
        check.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.54);
        check.lineTo(box.left() + box.width() * 0.42, box.top() + box.height() * 0.72);
        check.lineTo(box.left() + box.width() * 0.78, box.top() + box.height() * 0.30);
        // Fixed dark ink, not a palette token: this glyph sits on the raw accent fill
        // (see WindowButton's close-hover glyph for the same reasoning).
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

    // Logo, or a lettered placeholder when the manifest carried none.
    const qreal logoX = box.right() + CheckGap;
    const qreal logoY = std::round((height() - LogoSize) / 2.0);
    const QRectF logoBox(logoX, logoY, LogoSize, LogoSize);
    if (!m_logo.isNull()) {
        const qreal dpr = devicePixelRatioF();
        QPixmap scaled = m_logo.scaled(QSize(qRound(LogoSize * dpr), qRound(LogoSize * dpr)),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        const qreal dx = logoBox.left() + (LogoSize - scaled.width() / dpr) / 2.0;
        const qreal dy = logoBox.top() + (LogoSize - scaled.height() / dpr) / 2.0;
        p.drawPixmap(QPointF(dx, dy), scaled);
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(Color::Tile());
        p.drawRoundedRect(logoBox, Metric::ControlRadius, Metric::ControlRadius);
        const QString letter = m_name.isEmpty() ? QStringLiteral("?") : m_name.left(1).toUpper();
        Css::drawCentered(&p, logoBox, Font::blockTitle(), Color::TextSecondary(), letter,
                          Qt::AlignHCenter);
    }

    // Name / description (or status, once a removal has run).
    const qreal textX = logoBox.right() + LogoGap;
    const qreal textRight = m_removeButton->x() - ButtonGap;
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
