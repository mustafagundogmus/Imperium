#include "featurerow.h"

#include "../css.h"
#include "../fluent/fluenticons.h"
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
constexpr qreal IconBox = 30.0;
constexpr qreal IconSize = 16.0;
constexpr qreal IconGap = 12.0;
constexpr qreal ButtonGap = 14.0;
constexpr qreal TextGap = 1.0;
constexpr qreal StateGap = 6.0;

} // namespace

FeatureRow::FeatureRow(const QString &key, const Icons::Glyph *glyph, QWidget *parent)
    : QWidget(parent)
    , m_key(key)
    , m_glyph(glyph)
{
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::ArrowCursor);
    setFixedHeight(qRound(RowHeight));

    m_button = new PillButton(PillButton::Ghost, QString(), this);
    m_button->hide();

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        placeButton();
        update();
    });
}

QSize FeatureRow::sizeHint() const
{
    return {0, qRound(RowHeight)};
}

void FeatureRow::setName(const QString &name)
{
    m_name = name;
    update();
}

void FeatureRow::setDesc(const QString &desc)
{
    m_desc = desc;
    update();
}

void FeatureRow::setState(const QString &text, Tone tone)
{
    m_state = text;
    m_tone = tone;
    update();
}

void FeatureRow::setChecked(bool on)
{
    if (!m_selectable)
        on = false;
    if (m_checked == on)
        return;
    m_checked = on;
    update();
}

void FeatureRow::setSelectable(bool on)
{
    if (m_selectable == on)
        return;
    m_selectable = on;
    if (!on)
        m_checked = false;
    update();
}

void FeatureRow::setBusy(bool busy)
{
    m_button->setEnabledLook(!busy);
}

void FeatureRow::setStatus(const QString &text)
{
    m_status = text;
    update();
}

void FeatureRow::setActionVisible(bool visible)
{
    m_actionVisible = visible;
    m_button->setVisible(visible);
    placeButton();
    update();
}

QRectF FeatureRow::checkboxRect() const
{
    const qreal y = std::round((height() - CheckSize) / 2.0);
    return QRectF(PadX, y, CheckSize, CheckSize);
}

void FeatureRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    placeButton();
}

void FeatureRow::placeButton()
{
    const QSize hint = m_button->sizeHint();
    m_button->resize(hint);
    m_button->move(qRound(width() - PadX - hint.width()),
                   qRound((height() - hint.height()) / 2.0));
}

void FeatureRow::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void FeatureRow::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void FeatureRow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void FeatureRow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (m_selectable && rect().contains(e->pos())) {
            m_checked = !m_checked;
            update();
            Q_EMIT toggled(m_key, m_checked);
        }
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void FeatureRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = rect();
    if (m_checked) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::accentSoft());
        p.drawRoundedRect(r.adjusted(0, 1, 0, -1), Metric::ControlRadius, Metric::ControlRadius);
    } else if (m_hovered && m_selectable) {
        p.setPen(Qt::NoPen);
        p.setBrush(Color::SurfaceHover());
        p.drawRoundedRect(r.adjusted(0, 1, 0, -1), Metric::ControlRadius, Metric::ControlRadius);
    }

    // Checkbox, or a dash where there is nothing to tick.
    const QRectF box = checkboxRect();
    if (!m_selectable) {
        QPen pen(Color::TextFaint(), 1.2);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(QPointF(box.left() + 3, box.center().y()), QPointF(box.right() - 3, box.center().y()));
    } else if (m_checked) {
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

    // The icon box, the Fluent rows' 32px square scaled to this row.
    const qreal iconX = box.right() + CheckGap;
    const qreal iconY = std::round((height() - IconBox) / 2.0);
    const QRectF iconBox(iconX, iconY, IconBox, IconBox);
    p.setPen(Qt::NoPen);
    p.setBrush(Color::Tile());
    p.drawRoundedRect(iconBox, 4.0, 4.0);
    if (m_glyph) {
        const qreal dpr = devicePixelRatioF();
        const QPixmap pm = FluentIcons::draw(*m_glyph, m_checked ? Theme::accentInk() : Color::TextSecondary(),
                                             int(IconSize), 1.75, dpr);
        p.drawPixmap(QPointF(iconBox.left() + (IconBox - IconSize) / 2.0,
                             iconBox.top() + (IconBox - IconSize) / 2.0), pm);
    }

    // Name / state + description (or the status line after a run).
    const qreal textX = iconBox.right() + IconGap;
    const qreal textRight = m_actionVisible ? m_button->x() - ButtonGap : width() - PadX;
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
                      m_selectable ? Color::TextPrimary() : Color::TextMuted(), m_name,
                      Qt::AlignLeft, true);

        qreal descX = textX;
        const qreal descBaseline = Css::baseline(descFont, top + nameLine + TextGap, descLine);
        if (!m_state.isEmpty() && m_status.isEmpty()) {
            QColor ink = Color::TextMuted();
            switch (m_tone) {
            case Tone::On:     ink = Theme::accentInk(); break;
            case Tone::Warn:   ink = Color::Warn(); break;
            case Tone::Danger: ink = Color::Danger(); break;
            case Tone::Muted:  break;
            }
            const QFont stateFont = Theme::sans(pixelSize(descFont), Weight::Medium);
            const qreal w = Css::textWidth(stateFont, m_state);
            Css::drawText(&p, QRectF(descX, 0, w, height()), descBaseline, stateFont, ink, m_state);
            descX += w + StateGap;
        }
        const QString &desc = m_status.isEmpty() ? m_desc : m_status;
        const QColor descColor = m_status.isEmpty() ? Color::TextDesc() : Theme::accentInk();
        if (textRight - descX > 0)
            Css::drawText(&p, QRectF(descX, 0, textRight - descX, height()), descBaseline, descFont,
                          descColor, desc, Qt::AlignLeft, true);
    }
}
