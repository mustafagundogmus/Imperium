#include "tweakrow.h"
#include "rangeslider.h"
#include "segmentedcontrol.h"
#include "toggleswitch.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QPainter>

namespace {

constexpr qreal BorderW = 1.0;   // `border:1px solid transparent`
constexpr qreal PadX = 6.0;
const qreal ToggleCol = Theme::Metric::ToggleWidth;
constexpr qreal ColGap = 12.0;
constexpr qreal TextGap = 1.0;   // flex column gap between name and desc

qreal textBlockHeight()
{
    return Css::rowNameLine() + TextGap + Css::rowDescLine();
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
    setFixedHeight(rowHeight(m_choice));
    // Every metric here comes out of the font and the density flag, so either of them
    // changing means measuring again. compactChanged had no listener anywhere in the app,
    // which is why "denser rows" only took effect on the next launch — the switch wrote
    // the setting, emitted the signal, and nothing was listening.
    const auto remeasure = [this] {
        setFixedHeight(rowHeight(m_choice));
        updateGeometry();
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

int TweakRow::rowHeight(bool choice)
{
    const qreal text = 2 * (BorderW + Css::rowPadY()) + textBlockHeight();
    if (!choice)
        return qRound(text);

    // The segmented control is taller than the two lines of text beside it.
    const qreal control = 2 * (BorderW + Css::rowPadY()) + SegmentedControl::controlHeight();
    return qRound(qMax(text, control));
}

QSize TweakRow::sizeHint() const
{
    return {0, rowHeight(m_choice)};
}

void TweakRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    positionToggle();
}

void TweakRow::positionToggle()
{
    if (QWidget *trailing = m_segments ? static_cast<QWidget *>(m_segments)
                                       : static_cast<QWidget *>(m_slider)) {
        const int x = qRound(width() - BorderW - PadX - trailing->width());
        const int y = qRound((height() - trailing->height()) / 2.0);
        trailing->move(qMax(0, x), y);
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

    const qreal top = BorderW + Css::rowPadY();

    // A switch keeps its own column and the text starts after it; a choice puts its
    // control at the far end, so the text starts at the padding and stops before it.
    const qreal textX = m_choice ? BorderW + PadX : BorderW + PadX + ToggleCol + ColGap;
    const QWidget *trailing = m_segments ? static_cast<const QWidget *>(m_segments)
                                         : static_cast<const QWidget *>(m_slider);
    const qreal reserved = trailing ? trailing->width() + ColGap : 0.0;
    const qreal textW = width() - textX - (BorderW + PadX) - reserved;
    if (textW <= 0)
        return;

    const QFont &nameFont = Font::tweakName();
    const QFont &descFont = Font::tweakDesc();

    const QRectF box(textX, 0, textW, height());

    Css::drawText(&p, box, Css::baseline(nameFont, top, Css::rowNameLine()), nameFont,
                  m_applicable ? Color::TextPrimary() : Color::TextFaint(),
                  m_name, Qt::AlignLeft, /*elide=*/true);

    // The reason replaces the description rather than crowding in beside it: a row that
    // does nothing here has nothing to explain about what it would do.
    const qreal descTop = top + Css::rowNameLine() + TextGap;
    const qreal descBaseline = Css::baseline(descFont, descTop, Css::rowDescLine());
    QRectF descBox = box;

    // A row with a price says so first, in colour, and the description follows on the
    // same line: "Bedeli var · Pano geçmişini …". The word is the whole badge — a dot or
    // an icon would need a legend, and the description is already elided at the width
    // the badge takes, so nothing is hidden that was not hidden before. The badge stays
    // off a row this build ignores: its line is the requirement, and greying a warning
    // beside "requires 24H2" would be two reasons competing for one line.
    if (m_applicable && !m_risk.isEmpty()) {
        const bool unsafe = m_risk == QLatin1String("unsafe");
        const QString badge = Locale::tr(unsafe ? QStringLiteral("tweak.risk.unsafe")
                                                : QStringLiteral("tweak.risk.cost"))
                              + QStringLiteral(" · ");
        const qreal badgeW = Css::textWidth(descFont, badge);
        if (badgeW < textW) {
            Css::drawText(&p, box, descBaseline, descFont,
                          unsafe ? Color::Danger() : Color::Warn(), badge, Qt::AlignLeft,
                          /*elide=*/false);
            descBox.setLeft(box.left() + badgeW);
        }
    }

    Css::drawText(&p, descBox, descBaseline, descFont,
                  m_applicable ? Color::TextDesc() : Color::TextFainter(),
                  m_applicable ? m_desc : m_requirement, Qt::AlignLeft, /*elide=*/true);
}
