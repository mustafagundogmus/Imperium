#include "tweakrow.h"
#include "rangeslider.h"
#include "segmentedcontrol.h"
#include "toggleswitch.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <cmath>

#include <QPainter>

namespace {

constexpr qreal BorderW = 1.0;   // `border:1px solid transparent`
constexpr qreal PadX = 6.0;
const qreal ToggleCol = Theme::Metric::ToggleWidth;
constexpr qreal ColGap = 12.0;
constexpr qreal TextGap = 1.0;   // flex column gap between name and desc

// The narrowest text column a control may leave beside itself before it goes under the
// text instead, and the gap between the text and a control put there.
constexpr qreal StackBelow = 240.0;
constexpr qreal StackGap = 6.0;

/// The border and the vertical padding, above and below.
qreal frameHeight()
{
    return 2 * (BorderW + Css::rowPadY());
}

} // namespace

TweakRow::TweakRow(const Tweak &tweak, AppState *state, QWidget *parent)
    : QWidget(parent)
    , m_id(tweak.id)
    , m_name(tweak.displayName())
    , m_desc(tweak.displayDesc())
    , m_risk(tweak.risk)
    , m_choice(tweak.isChoice || tweak.isRange)
    , m_applicable(tweak.editable())
    , m_requirement(tweak.blockReason())
    , m_state(state)
{
    // The height is a function of the width — see the header — and the layout has to be
    // told so, or it takes sizeHint's single line and never asks.
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    // Every metric here comes out of the font and the density flag, so either of them
    // changing means measuring again. compactChanged had no listener anywhere in the app,
    // which is why "denser rows" only took effect on the next launch — the switch wrote
    // the setting, emitted the signal, and nothing was listening.
    const auto remeasure = [this] {
        updateGeometry();
        positionToggle();
        update();
    };
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, remeasure);
    connect(Theme::notifier(), &Theme::Notifier::compactChanged, this, remeasure);

    // A tweak this build ignores is shown, not hidden — knowing it exists and does not
    // apply here is worth more than a shorter list — but it cannot be operated.
    if (tweak.isRange) {
        QStringList labels;
        labels.reserve(tweak.options.size());
        for (const TweakOption &option : tweak.options)
            labels << option.displayLabel();

        m_slider = new RangeSlider(labels, this);
        m_slider->setCurrentIndex(state->selected(m_id));
        connect(m_slider, &RangeSlider::currentIndexChanged, this, [this](int index) {
            m_state->setSelected(m_id, index);
        });
    } else if (tweak.isChoice) {
        // A choice needs room for its labels, so it sits at the end of the row rather
        // than in the switch's 30px column — the same arrangement the settings page uses
        // for a control that cannot be reduced to a pill.
        QStringList labels;
        labels.reserve(tweak.options.size());
        for (const TweakOption &option : tweak.options)
            labels << option.displayLabel();

        m_segments = new SegmentedControl(labels, this);
        m_segments->setCurrentIndex(state->selected(m_id));
        connect(m_segments, &SegmentedControl::currentIndexChanged, this, [this](int index) {
            m_state->setSelected(m_id, index);
        });
    } else {
        m_toggle = new ToggleSwitch(this);
        m_toggle->setChecked(state->selected(m_id) == 1, /*animate=*/false);
        connect(m_toggle, &ToggleSwitch::toggled, this, [this](bool on) {
            m_state->setOn(m_id, on);
        });
    }

    // Keep the control honest when something else moves the state (Geri al, Uygula).
    connect(state, &AppState::tweakToggled, this, [this](const QString &id) {
        if (id != m_id)
            return;
        if (m_segments)
            m_segments->setCurrentIndex(m_state->selected(m_id));
        else if (m_slider)
            m_slider->setCurrentIndex(m_state->selected(m_id));
        else
            m_toggle->setChecked(m_state->selected(m_id) == 1);
    });

    if (!tweak.tooltip.isEmpty())
        setToolTip(tweak.tooltip);

    if (!m_applicable) {
        if (m_segments)
            m_segments->setEnabled(false);
        if (m_slider)
            m_slider->setEnabled(false);
        if (m_toggle)
            m_toggle->setEnabled(false);
    }

    // Nothing in this row is a stock widget. The name and the description are drawn in
    // paintEvent and the control beside them is custom too, so to a screen reader the row
    // is an unnamed rectangle containing an unnamed thing — setAccessibleName appeared
    // nowhere in src/ before this. The strings cost nothing to supply: the row is already
    // holding them in order to paint them. The control is named as well as the row because
    // the control is what takes focus, and a row that cannot say which switch is focused
    // is a poor thing to hand somebody in a program that writes to the registry as
    // administrator. A row that cannot be operated describes itself by the reason it
    // cannot, which is what its greyed-out state and requirement line say to everyone else.
    const QString detail = (m_applicable || m_requirement.isEmpty()) ? m_desc : m_requirement;
    setAccessibleName(m_name);
    setAccessibleDescription(detail);
    for (QWidget *control : {static_cast<QWidget *>(m_toggle),
                             static_cast<QWidget *>(m_segments),
                             static_cast<QWidget *>(m_slider)}) {
        if (!control)
            continue;
        control->setAccessibleName(m_name);
        control->setAccessibleDescription(detail);
    }

    positionToggle();
}

QWidget *TweakRow::trailing() const
{
    // The control that ends the row: a choice's segments or a range's slider. A switch
    // is not one; it keeps its own column at the start.
    if (m_segments)
        return m_segments;
    return m_slider;
}

TweakRow::TextLayout TweakRow::measure(int width) const
{
    TextLayout t;

    // A switch keeps its own column and the text starts after it; a choice puts its
    // control at the far end, so the text starts at the padding and stops before it —
    // while the text keeps the wider share of the row and a column worth reading. Past
    // that point the control goes under the text and the text takes the whole width: a
    // four-way choice with sentence-long labels otherwise leaves the description a dozen
    // lines of four words each.
    t.x = m_choice ? BorderW + PadX : BorderW + PadX + ToggleCol + ColGap;
    const QWidget *control = trailing();
    const qreal natural = control ? control->sizeHint().width() : 0.0;
    const qreal right = width - (BorderW + PadX);
    const qreal beside = control ? right - natural - ColGap - t.x : right - t.x;
    t.stacked = control && (natural > beside || beside < StackBelow);
    t.width = t.stacked ? right - t.x : beside;
    if (control) {
        // Under the text the control gets the column's width, and a segmented control
        // breaks its segments into lines to fit it; beside the text it keeps its own.
        t.controlAvail = t.stacked ? std::floor(t.width) : 0.0;
        t.controlH = (t.stacked && m_segments) ? m_segments->heightForWidth(int(t.controlAvail))
                                               : control->sizeHint().height();
    }

    const QFont &nameFont = Theme::Font::tweakName();
    const QFont &descFont = Theme::Font::tweakDesc();

    t.nameLines = Css::wrapLines(nameFont, m_name, t.width);

    // A row with a price says so first, in colour, and the description follows on the
    // same line and wraps under it: "Bedeli var · Pano geçmişini …". The word is the
    // whole badge — a dot or an icon would need a legend. The badge stays off a row this
    // build ignores: its line is the requirement, and greying a warning beside "requires
    // 24H2" would be two reasons competing for one line.
    if (m_applicable && !m_risk.isEmpty()) {
        const bool unsafe = m_risk == QLatin1String("unsafe");
        const QString badge = Locale::tr(unsafe ? QStringLiteral("tweak.risk.unsafe")
                                                : QStringLiteral("tweak.risk.cost"))
                              + QStringLiteral(" · ");
        const qreal badgeW = Css::textWidth(descFont, badge);
        if (badgeW < t.width) {
            t.badge = badge;
            t.badgeWidth = badgeW;
            t.badgeColor = unsafe ? Theme::Color::Danger() : Theme::Color::Warn();
        }
    }

    // The reason replaces the description rather than crowding in beside it: a row that
    // does nothing here has nothing to explain about what it would do.
    t.descLines = Css::wrapLines(descFont, m_applicable ? m_desc : m_requirement, t.width,
                                 t.width - t.badgeWidth);

    t.blockHeight = t.nameLines.size() * Css::rowNameLine() + TextGap
                    + t.descLines.size() * Css::rowDescLine();
    return t;
}

int TweakRow::heightForWidth(int width) const
{
    const TextLayout t = measure(width);
    const qreal text = frameHeight() + t.blockHeight;
    if (!trailing())
        return qRound(text);
    if (t.stacked)
        return qRound(text + StackGap + t.controlH);

    // The control beside the text can be taller than the text.
    return qRound(qMax(text, frameHeight() + t.controlH));
}

QSize TweakRow::sizeHint() const
{
    const qreal text = frameHeight() + Css::rowNameLine() + TextGap + Css::rowDescLine();
    const QWidget *control = trailing();
    if (!control)
        return {0, qRound(text)};
    return {0, qRound(qMax(text, frameHeight() + control->sizeHint().height()))};
}

void TweakRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    positionToggle();
}

void TweakRow::positionToggle()
{
    if (QWidget *control = trailing()) {
        const TextLayout t = measure(width());
        if (m_segments)
            m_segments->setAvailableWidth(t.controlAvail);
        if (t.stacked) {
            // Under the text, flush with its left edge.
            control->move(qRound(t.x), qRound(BorderW + Css::rowPadY() + t.blockHeight + StackGap));
        } else {
            const int x = qRound(width() - BorderW - PadX - control->width());
            const int y = qRound((height() - control->height()) / 2.0);
            control->move(qMax(0, x), y);
        }
        return;
    }

    // `align-items:center` centres the pill in the content box.
    const int x = qRound(BorderW + PadX);
    const int y = qRound((height() - m_toggle->height()) / 2.0);
    m_toggle->move(x, y);
}

void TweakRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const TextLayout t = measure(width());
    if (t.width <= 0)
        return;

    const QFont &nameFont = Font::tweakName();
    const QFont &descFont = Font::tweakDesc();
    const QRectF box(t.x, 0, t.width, height());

    // The name, one line box per wrapped line, from the top of the content box.
    const qreal nameLine = Css::rowNameLine();
    qreal lineTop = BorderW + Css::rowPadY();
    for (const QString &line : t.nameLines) {
        Css::drawText(&p, box, Css::baseline(nameFont, lineTop, nameLine), nameFont,
                      m_applicable ? Color::TextPrimary() : Color::TextFaint(), line,
                      Qt::AlignLeft, /*elide=*/false);
        lineTop += nameLine;
    }

    // The description the same way, stacked at its own line height. The badge shares
    // the first of its lines, and that line was wrapped at the width it leaves.
    const qreal descLine = Css::rowDescLine();
    const QColor descColor = m_applicable ? Color::TextDesc() : Color::TextFainter();
    lineTop += TextGap;
    for (int i = 0; i < t.descLines.size(); ++i, lineTop += descLine) {
        const qreal baseline = Css::baseline(descFont, lineTop, descLine);
        QRectF lineBox = box;
        if (i == 0 && !t.badge.isEmpty()) {
            Css::drawText(&p, box, baseline, descFont, t.badgeColor, t.badge, Qt::AlignLeft,
                          /*elide=*/false);
            lineBox.setLeft(box.left() + t.badgeWidth);
        }
        Css::drawText(&p, lineBox, baseline, descFont, descColor, t.descLines.at(i),
                      Qt::AlignLeft, /*elide=*/false);
    }
}
