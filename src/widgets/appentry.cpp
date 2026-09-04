#include "appentry.h"

#include "../css.h"
#include "../theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr qreal PadX = 10.0;
constexpr qreal PadY = 8.0;
constexpr qreal CheckSize = 14.0;
constexpr qreal CheckGap = 9.0;
constexpr qreal IconSize = 22.0;
constexpr qreal IconGap = 9.0;
constexpr qreal StatusGap = 8.0;
constexpr qreal BadgeSize = 18.0;

// WinUtil's badge colours: the FOSS green and the near-white keyhole, fixed rather than
// palette tokens because the badge has to read as the same mark on every scheme.
const QColor FossGreen(19, 143, 83);
const QColor FossInk(247, 247, 247);

} // namespace

AppEntry::AppEntry(const QString &key, const QString &name, bool foss, QWidget *parent)
    : QWidget(parent)
    , m_key(key)
    , m_name(name)
    , m_foss(foss)
{
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setFixedSize(tileWidth(), tileHeight());

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedSize(tileWidth(), tileHeight());
        updateGeometry();
        update();
    });
}

int AppEntry::tileWidth()
{
    // Sized from the name font so that "Visual Studio Code (Insiders)" fits at every
    // interface scale; the page's flow decides how many go on a line — four across at the
    // window's opening width in either shell, which is how WinUtil's tab reads too.
    return qRound(Css::textWidth(Theme::Font::tweakName(), QStringLiteral("0")) * 28.0);
}

int AppEntry::tileHeight()
{
    const qreal line = qMax(IconSize, Css::normalLine(Theme::Font::tweakName()));
    return qRound(2 * PadY + line);
}

QSize AppEntry::sizeHint() const
{
    return {tileWidth(), tileHeight()};
}

void AppEntry::setChecked(bool on)
{
    if (m_checked == on)
        return;
    m_checked = on;
    update();
}

void AppEntry::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    update();
}

void AppEntry::setStatus(const QString &text, bool good)
{
    m_status = text;
    m_statusGood = good;
    update();
}

void AppEntry::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void AppEntry::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void AppEntry::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void AppEntry::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (!m_busy && rect().contains(e->pos())) {
            m_checked = !m_checked;
            update();
            Q_EMIT toggled(m_key, m_checked);
        }
        return;
    }
    if (e->button() == Qt::RightButton) {
        e->accept();
        if (rect().contains(e->pos()))
            Q_EMIT contextRequested(m_key, e->globalPosition().toPoint());
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void AppEntry::keyPressEvent(QKeyEvent *e)
{
    if (!m_busy && (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return)) {
        m_checked = !m_checked;
        update();
        Q_EMIT toggled(m_key, m_checked);
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_Menu) {
        Q_EMIT contextRequested(m_key, mapToGlobal(rect().center()));
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void AppEntry::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    if (m_busy)
        p.setOpacity(0.55);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = Metric::ControlRadius;

    // The tile: WinUtil's AppInstallUnselectedColor / Highlighted / Selected.
    QColor fill = Color::Tile();
    QColor border = Color::TileBorder();
    if (m_checked) {
        fill = Theme::accentSoft();
        border = Theme::accent();
    } else if (m_hovered && !m_busy) {
        fill = Color::SurfaceHover();
        border = Color::BorderControl();
    }
    if (hasFocus())
        border = Theme::accent();

    QPainterPath tile;
    tile.addRoundedRect(r, radius, radius);
    p.setPen(QPen(border, 1.0));
    p.setBrush(fill);
    p.drawPath(tile);

    // Checkbox.
    const qreal checkY = std::round((height() - CheckSize) / 2.0);
    const QRectF box(PadX, checkY, CheckSize, CheckSize);
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
        p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), Metric::BadgeRadius,
                          Metric::BadgeRadius);
    }

    // Icon box with the initial — WinUtil's fallback glyph, drawn in the toggle-on colour.
    const qreal iconX = box.right() + CheckGap;
    const qreal iconY = std::round((height() - IconSize) / 2.0);
    const QRectF iconBox(iconX, iconY, IconSize, IconSize);
    p.setPen(Qt::NoPen);
    p.setBrush(m_checked ? Theme::accent() : Color::Surface());
    p.drawRoundedRect(iconBox, 4.0, 4.0);
    QString initial = m_name;
    while (initial.startsWith(QLatin1Char('.')))
        initial.remove(0, 1);
    initial = initial.isEmpty() ? QStringLiteral("?") : initial.left(1).toUpper();
    Css::drawCentered(&p, iconBox, Theme::sans(11.5, Weight::SemiBold),
                      m_checked ? Color::OnAccent() : Theme::accentInk(), initial,
                      Qt::AlignHCenter);

    // Status word at the right, when there is one.
    qreal right = r.right() - PadX;
    if (!m_status.isEmpty()) {
        const QFont &statusFont = Font::sectionCount();
        const qreal w = Css::textWidth(statusFont, m_status);
        const QRectF statusBox(right - w, 0, w, height());
        Css::drawCentered(&p, statusBox, statusFont,
                          m_statusGood ? Theme::accentInk() : Color::Danger(), m_status,
                          Qt::AlignRight);
        right -= w + StatusGap;
    } else if (m_foss) {
        right -= BadgeSize * 0.45;
    }

    // Name.
    const qreal textX = iconBox.right() + IconGap;
    const QRectF textBox(textX, 0, qMax(0.0, right - textX), height());
    Css::drawCentered(&p, textBox, Font::tweakName(),
                      m_checked ? Color::TextPrimary() : Color::TextSecondary(), m_name,
                      Qt::AlignLeft, true);

    // The FOSS badge: WinUtil's New-WinUtilFossBadge — a triangle filling the top-right
    // corner, its outer corner rounded to match the tile, with the keyhole centred on it.
    if (m_foss) {
        p.save();
        p.setClipPath(tile);
        const qreal s = BadgeSize / 22.0;
        p.translate(r.right() - BadgeSize + 0.5, r.top() - 0.5);
        p.scale(s, s);
        QPainterPath backdrop;
        backdrop.moveTo(0, 0);
        backdrop.lineTo(17, 0);
        backdrop.arcTo(QRectF(12, 0, 10, 10), 90, -90);   // "A 5,5 0 0 1 22,5"
        backdrop.lineTo(22, 22);
        backdrop.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(FossGreen);
        p.drawPath(backdrop);
        // "M 13.61,9.225 A 3.4,3.4 0 1 1 17.51,9.225": the ring, open at its foot.
        QPen ink(FossInk, 2.4);
        ink.setCapStyle(Qt::RoundCap);
        p.setPen(ink);
        p.setBrush(Qt::NoBrush);
        const QPointF centre(15.56, 6.44);
        p.drawArc(QRectF(centre.x() - 3.4, centre.y() - 3.4, 6.8, 6.8), 235 * 16, -290 * 16);
        p.restore();
    }
}
