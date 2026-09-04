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

#include <cmath>

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

// The narrowest text column a control may leave beside itself before it goes under the
// text instead, and the gap between the text and a control put there.
constexpr qreal StackBelow = 240.0;
constexpr qreal StackGap = 8.0;

QFont nameFont()
{
    return Theme::sans(14, Theme::Weight::Medium);
}

QFont descFont()
{
    return Theme::sans(12);
}

QFont badgeFont()
{
    return Theme::sans(10, Theme::Weight::SemiBold);
}
} // namespace

struct FluentTweakRow::Layout
{
    struct Badge
    {
        QString text;
        QColor bg;
        QColor fg;
        qreal width = 0.0;   ///< padding included
    };

    qreal textX = 0.0;        ///< left edge of the text column
    qreal textW = 0.0;        ///< its width
    bool stacked = false;     ///< the control is under the text rather than beside it
    QStringList nameLines;    ///< the name wrapped at textW
    QVector<Badge> badges;    ///< what follows the name
    bool badgeLine = false;   ///< the badges did not fit after the name's last line
    QStringList descLines;    ///< the description wrapped at textW
    qreal blockHeight = 0.0;  ///< name lines, the badge line, the gap, description lines
    qreal controlAvail = 0.0; ///< width the control may take under the text; 0 beside it
    qreal controlH = 0.0;     ///< the control's height in the place it gets

    /// The band the icon box and the text block share, each centred in it.
    qreal region() const { return qMax(IconBox, blockHeight); }
};

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

    // The height follows the width — see tweakrow.h — and the layout has to be told so,
    // or it takes sizeHint's single line and never asks.
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

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
    // once without touching any single row — and a badge appearing can push the others
    // onto a line of their own, so the row measures again rather than only repainting.
    connect(state, &AppState::pendingChanged, this, [this] {
        updateGeometry();
        update();
    });

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

    // The control measures itself on these two signals first — its connections were made
    // in its constructor, above — so the row sees the new width when it moves it.
    const auto remeasure = [this] {
        updateGeometry();
        positionControl();
        update();
    };
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, remeasure);
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, remeasure);
    positionControl();
}

FluentTweakRow::Layout FluentTweakRow::measure(int width) const
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    Layout l;
    l.textX = PadX + IconBox + Gap;
    const qreal right = width - PadX;

    // The control sits beside the text while the text keeps the wider share of the row
    // and a column worth reading, and goes under it otherwise: a four-way choice with
    // sentence-long labels took most of a wide window and left the description a dozen
    // lines of four words each, squeezed against the icon.
    const QWidget *c = control();
    const qreal natural = c ? c->sizeHint().width() : 0.0;
    const qreal beside = c ? right - natural - Gap - l.textX : right - l.textX;
    l.stacked = c && (natural > beside || beside < StackBelow);
    l.textW = l.stacked ? right - l.textX : beside;
    if (c) {
        // Under the text the control gets the column's width, and a segmented control
        // breaks its segments into lines to fit it; beside the text it keeps its own.
        l.controlAvail = l.stacked ? std::floor(l.textW) : 0.0;
        l.controlH = (l.stacked && m_segments) ? m_segments->heightForWidth(int(l.controlAvail))
                                               : c->sizeHint().height();
    }

    const QFont name = nameFont();
    l.nameLines = Css::wrapLines(name, m_name, l.textW);

    if (!m_applicable) {
        l.badges.append({Locale::tr(QStringLiteral("fluent.badge.na")), t.track, t.textMuted});
    } else if (m_locked) {
        l.badges.append({Locale::tr(QStringLiteral("fluent.badge.locked")), t.track, t.textMuted});
    } else if (m_state->isPending(m_id)) {
        l.badges.append({Locale::tr(QStringLiteral("fluent.badge.pending")), t.accentSoft, t.accentText});
    }
    if (m_applicable && !m_risk.isEmpty()) {
        const bool unsafe = m_risk == QLatin1String("unsafe");
        QColor ink = unsafe ? Theme::Color::Danger() : Theme::Color::Warn();
        QColor wash = ink;
        wash.setAlpha(0x26);
        l.badges.append({Css::upperTr(Locale::tr(unsafe ? QStringLiteral("tweak.risk.unsafe")
                                                        : QStringLiteral("tweak.risk.cost"))),
                         wash, ink});
    }
    qreal badgesW = 0.0;
    const QFont badge = badgeFont();
    for (Layout::Badge &b : l.badges) {
        b.width = 2 * BadgePadX + Css::textWidth(badge, b.text);
        badgesW += BadgeGap + b.width;
    }
    // After the name's last line when they fit there, on a line of their own when not:
    // a badge that did not fit used to be dropped, which is the one thing a badge that
    // says "this build ignores this row" must not be.
    if (badgesW > 0.0)
        l.badgeLine = Css::textWidth(name, l.nameLines.last()) + badgesW > l.textW;

    // The description, or the reason the row cannot be operated.
    l.descLines = Css::wrapLines(descFont(), (m_applicable && !m_locked) ? m_desc : m_blockReason,
                                 l.textW);

    l.blockHeight = (l.nameLines.size() + (l.badgeLine ? 1 : 0)) * Css::line(name, 1.4) + TextGap
                    + l.descLines.size() * Css::line(descFont(), 1.4);
    return l;
}

int FluentTweakRow::heightForWidth(int width) const
{
    // The icon box sets the floor: 12 + 32 + 12. Two text lines (14px at 1.4 and 12px at
    // 1.4, 2px apart) come to 38.4 and sit centred inside it; a third line, or a larger
    // interface scale, lets the text win and the row grows with it. A control under the
    // text adds its own height below the band.
    const Layout l = measure(width);
    qreal h = 2 * PadY + l.region();
    if (l.stacked)
        h += StackGap + l.controlH;
    return qRound(h);
}

QSize FluentTweakRow::sizeHint() const
{
    const qreal text = Css::line(nameFont(), 1.4) + TextGap + Css::line(descFont(), 1.4);
    return {0, qRound(2 * PadY + qMax(IconBox, text))};
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
    const Layout l = measure(width());
    if (m_segments)
        m_segments->setAvailableWidth(l.controlAvail);
    if (l.stacked)
        c->move(qRound(l.textX), qRound(PadY + l.region() + StackGap));
    else
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

    const Layout l = measure(width());

    // Icon box, centred on the band it shares with the text.
    const qreal boxY = std::round(PadY + (l.region() - IconBox) / 2.0);
    p.setPen(Qt::NoPen);
    p.setBrush(t.iconBg);
    p.drawRoundedRect(QRectF(PadX, boxY, IconBox, IconBox), IconRadius, IconRadius);
    if (m_glyph) {
        const QPixmap glyph = FluentIcons::draw(*m_glyph, t.textSec, IconSize, 1.75, devicePixelRatioF());
        p.drawPixmap(QPointF(PadX + std::round((IconBox - IconSize) / 2.0),
                             boxY + std::round((IconBox - IconSize) / 2.0)),
                     glyph);
    }

    if (l.textW <= 0)
        return;

    const QFont name = nameFont();
    const QFont desc = descFont();
    const QFont badge = badgeFont();
    const qreal nameLine = Css::line(name, 1.4);
    const qreal descLine = Css::line(desc, 1.4);
    const QRectF column(l.textX, 0, l.textW, height());

    // The name, one line box per wrapped line, the block centred on the band.
    qreal lineTop = PadY + (l.region() - l.blockHeight) / 2.0;
    for (const QString &line : l.nameLines) {
        Css::drawText(&p, column, Css::baseline(name, lineTop, nameLine), name, t.text, line,
                      Qt::AlignLeft, /*elide=*/false);
        lineTop += nameLine;
    }

    // The badges: after the name's last line, or on the line that follows it.
    if (!l.badges.isEmpty()) {
        qreal bx = l.textX;
        qreal badgeTop = lineTop - nameLine;
        if (l.badgeLine) {
            badgeTop = lineTop;
            lineTop += nameLine;
        } else {
            bx += Css::textWidth(name, l.nameLines.last());
        }
        const qreal badgeH = Css::normalLine(badge) + 2 * BadgePadY;
        const qreal badgeY = badgeTop + (nameLine - badgeH) / 2.0;
        bool first = l.badgeLine;   // on its own line the first badge starts at the column
        for (const Layout::Badge &b : l.badges) {
            if (!first)
                bx += BadgeGap;
            first = false;
            if (bx + b.width > column.right())
                break;
            p.setPen(Qt::NoPen);
            p.setBrush(b.bg);
            p.drawRoundedRect(QRectF(bx, badgeY, b.width, badgeH), BadgeRadius, BadgeRadius);
            Css::drawCentered(&p, QRectF(bx, badgeY, b.width, badgeH), badge, b.fg, b.text,
                              Qt::AlignHCenter);
            bx += b.width;
        }
    }

    // The description, one line box per wrapped line.
    lineTop += TextGap;
    for (const QString &line : l.descLines) {
        Css::drawText(&p, column, Css::baseline(desc, lineTop, descLine), desc, t.textSec, line,
                      Qt::AlignLeft, /*elide=*/false);
        lineTop += descLine;
    }
}
