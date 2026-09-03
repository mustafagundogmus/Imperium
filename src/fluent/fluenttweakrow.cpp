#include "fluenttweakrow.h"
#include "fluenticons.h"
#include "fluentslider.h"
#include "fluenttoggle.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../css.h"
#include "../i18n.h"
#include "../icons.h"
#include "../theme.h"
#include "../widgets/segmentedcontrol.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr qreal PadX = 16.0;
constexpr qreal PadY = 12.0;
constexpr qreal Gap = 16.0;
constexpr qreal IconBox = 32.0;
constexpr int IconSize = 16;
constexpr qreal IconRadius = 4.0;
constexpr qreal TextGap = 2.0;
constexpr qreal BadgeGap = 8.0;
constexpr qreal BadgePadX = 6.0;
constexpr qreal BadgePadY = 1.0;
constexpr qreal BadgeRadius = 3.0;
constexpr qreal CardRadius = 6.0;
constexpr qreal BlockedOpacity = 0.55;

struct Badge
{
    QString text;
    QColor bg;
    QColor fg;
};
} // namespace

FluentTweakRow::FluentTweakRow(const Tweak &tweak, AppState *state, const QString &categoryId,
                               QWidget *parent)
    : QWidget(parent)
    , m_id(tweak.id)
    , m_name(tweak.displayName())
    , m_desc(tweak.displayDesc())
    , m_risk(tweak.risk)
    , m_glyph(&FluentIcons::tweakGlyph(tweak, categoryId))
    , m_applicable(tweak.applicable)
    , m_locked(tweak.locked)
    , m_blockReason(tweak.blockReason())
    , m_state(state)
{
    setAttribute(Qt::WA_Hover, true);
    setFixedHeight(rowHeight());

    if (tweak.isRange) {
        QStringList labels;
        for (const TweakOption &option : tweak.options)
            labels << option.displayLabel();
        m_slider = new FluentSlider(labels, this);
        m_slider->setCurrentIndex(state->selected(m_id));
        connect(m_slider, &FluentSlider::currentIndexChanged, this,
                [this](int index) { m_state->setSelected(m_id, index); });
    } else if (tweak.isChoice) {
        QStringList labels;
        for (const TweakOption &option : tweak.options)
            labels << option.displayLabel();
        m_segments = new SegmentedControl(labels, this);
        m_segments->setCurrentIndex(state->selected(m_id));
        connect(m_segments, &SegmentedControl::currentIndexChanged, this,
                [this](int index) { m_state->setSelected(m_id, index); });
    } else {
        m_toggle = new FluentToggle(this);
        m_toggle->setChecked(state->selected(m_id) == 1, /*animate=*/false);
        connect(m_toggle, &FluentToggle::toggled, this, [this](bool on) { m_state->setOn(m_id, on); });
    }

    connect(state, &AppState::tweakToggled, this, [this](const QString &id) {
        if (id != m_id)
            return;
        if (m_segments)
            m_segments->setCurrentIndex(m_state->selected(m_id));
        else if (m_slider)
            m_slider->setCurrentIndex(m_state->selected(m_id));
        else
            m_toggle->setChecked(m_state->selected(m_id) == 1);
        update();
    });
    // The BEKLİYOR badge is a function of the pending set, which an apply empties all at
    // once without touching any single row.
    connect(state, &AppState::pendingChanged, this, qOverload<>(&QWidget::update));

    if (!tweak.tooltip.isEmpty())
        setToolTip(tweak.tooltip);

    if (!tweak.editable()) {
        if (m_segments) m_segments->setEnabled(false);
        if (m_slider) m_slider->setEnabled(false);
        if (m_toggle) m_toggle->setEnabled(false);
    }

    const QString detail = tweak.editable() ? m_desc : m_blockReason;
    setAccessibleName(m_name);
    setAccessibleDescription(detail);
    if (QWidget *c = control()) {
        c->setAccessibleName(m_name);
        c->setAccessibleDescription(detail);
    }

    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedHeight(rowHeight());
        positionControl();
        update();
    });
    positionControl();
}

int FluentTweakRow::rowHeight()
{
    // The icon box sets the height: 12 + 32 + 12. The two text lines (14px at 1.4 and 12px
    // at 1.4, 2px apart) come to 38.4 and sit centred inside it; a larger interface scale
    // lets the text win.
    const qreal text = Css::line(Theme::sans(14, Theme::Weight::Medium), 1.4) + TextGap
                       + Css::line(Theme::sans(12), 1.4);
    return qRound(2 * PadY + qMax(IconBox, text));
}

QSize FluentTweakRow::sizeHint() const
{
    return {0, rowHeight()};
}

void FluentTweakRow::setEdges(bool first, bool last)
{
    m_first = first;
    m_last = last;
}

QWidget *FluentTweakRow::control() const
{
    if (m_toggle)
        return m_toggle;
    if (m_slider)
        return m_slider;
    return m_segments;
}

void FluentTweakRow::positionControl()
{
    QWidget *c = control();
    if (!c)
        return;
    c->move(qRound(width() - PadX - c->width()), qRound((height() - c->height()) / 2.0));
}

void FluentTweakRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    positionControl();
}

void FluentTweakRow::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void FluentTweakRow::leaveEvent(QEvent *e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void FluentTweakRow::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_hovered) {
        // `overflow:hidden` on the card: the first and last rows round the corners the
        // card rounds, the ones between are square.
        QPainterPath path;
        const QRectF r = rect();
        const qreal top = m_first ? CardRadius : 0.0;
        const qreal bottom = m_last ? CardRadius : 0.0;
        path.moveTo(r.left() + top, r.top());
        path.lineTo(r.right() - top, r.top());
        if (top > 0) path.arcTo(r.right() - 2 * top, r.top(), 2 * top, 2 * top, 90, -90);
        path.lineTo(r.right(), r.bottom() - bottom);
        if (bottom > 0) path.arcTo(r.right() - 2 * bottom, r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom, 0, -90);
        path.lineTo(r.left() + bottom, r.bottom());
        if (bottom > 0) path.arcTo(r.left(), r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom, 270, -90);
        path.lineTo(r.left(), r.top() + top);
        if (top > 0) path.arcTo(r.left(), r.top(), 2 * top, 2 * top, 180, -90);
        path.closeSubpath();
        p.fillPath(path, t.rowHover);
    }

    if (!m_applicable)
        p.setOpacity(BlockedOpacity);

    // Icon box.
    const qreal boxY = std::round((height() - IconBox) / 2.0);
    p.setPen(Qt::NoPen);
    p.setBrush(t.iconBg);
    p.drawRoundedRect(QRectF(PadX, boxY, IconBox, IconBox), IconRadius, IconRadius);
    if (m_glyph) {
        const QPixmap glyph = FluentIcons::draw(*m_glyph, t.textSec, IconSize, 1.75, devicePixelRatioF());
        p.drawPixmap(QPointF(PadX + std::round((IconBox - IconSize) / 2.0),
                             boxY + std::round((IconBox - IconSize) / 2.0)),
                     glyph);
    }

    const QWidget *c = control();
    const qreal textX = PadX + IconBox + Gap;
    const qreal textRight = c ? c->x() - Gap : width() - PadX;
    const qreal textW = textRight - textX;
    if (textW <= 0)
        return;

    const QFont nameFont = Theme::sans(14, Theme::Weight::Medium);
    const QFont descFont = Theme::sans(12);
    const QFont badgeFont = Theme::sans(10, Theme::Weight::SemiBold);
    const qreal nameLine = Css::line(nameFont, 1.4);
    const qreal descLine = Css::line(descFont, 1.4);
    const qreal blockTop = (height() - (nameLine + TextGap + descLine)) / 2.0;

    // The badges are measured first so the name can be elided to the room they leave.
    QVector<Badge> badges;
    if (!m_applicable) {
        badges.append({Locale::tr(QStringLiteral("fluent.badge.na")), t.track, t.textMuted});
    } else if (m_locked) {
        badges.append({Locale::tr(QStringLiteral("fluent.badge.locked")), t.track, t.textMuted});
    } else if (m_state->isPending(m_id)) {
        badges.append({Locale::tr(QStringLiteral("fluent.badge.pending")), t.accentSoft, t.accentText});
    }
    if (m_applicable && !m_risk.isEmpty()) {
        const bool unsafe = m_risk == QLatin1String("unsafe");
        QColor ink = unsafe ? Theme::Color::Danger() : Theme::Color::Warn();
        QColor wash = ink;
        wash.setAlpha(0x26);
        badges.append({Css::upperTr(Locale::tr(unsafe ? QStringLiteral("tweak.risk.unsafe")
                                                      : QStringLiteral("tweak.risk.cost"))),
                       wash, ink});
    }
    qreal badgesW = 0;
    for (const Badge &b : badges)
        badgesW += BadgeGap + 2 * BadgePadX + Css::textWidth(badgeFont, b.text);

    const qreal nameW = qMin(Css::textWidth(nameFont, m_name), qMax(0.0, textW - badgesW));
    const qreal nameBaseline = Css::baseline(nameFont, blockTop, nameLine);
    Css::drawText(&p, QRectF(textX, 0, nameW, height()), nameBaseline, nameFont, t.text, m_name,
                  Qt::AlignLeft, /*elide=*/true);

    qreal bx = textX + nameW;
    const qreal badgeH = Css::normalLine(badgeFont) + 2 * BadgePadY;
    const qreal badgeY = blockTop + (nameLine - badgeH) / 2.0;
    for (const Badge &b : badges) {
        bx += BadgeGap;
        const qreal bw = 2 * BadgePadX + Css::textWidth(badgeFont, b.text);
        if (bx + bw > textRight)
            break;
        p.setPen(Qt::NoPen);
        p.setBrush(b.bg);
        p.drawRoundedRect(QRectF(bx, badgeY, bw, badgeH), BadgeRadius, BadgeRadius);
        Css::drawCentered(&p, QRectF(bx, badgeY, bw, badgeH), badgeFont, b.fg, b.text, Qt::AlignHCenter);
        bx += bw;
    }

    // The description, or the reason the row cannot be operated.
    const QString line = (m_applicable && !m_locked) ? m_desc : m_blockReason;
    Css::drawText(&p, QRectF(textX, 0, textW, height()),
                  Css::baseline(descFont, blockTop + nameLine + TextGap, descLine), descFont,
                  t.textSec, line, Qt::AlignLeft, /*elide=*/true);
}
