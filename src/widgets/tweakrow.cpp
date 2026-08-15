#include "tweakrow.h"
#include "toggleswitch.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../css.h"
#include "../theme.h"

#include <QPainter>

namespace {

constexpr qreal BorderW = 1.0;   // `border:1px solid transparent`
constexpr qreal PadX = 6.0;
constexpr qreal ToggleCol = 26.0;
constexpr qreal ColGap = 12.0;
constexpr qreal TextGap = 1.0;   // flex column gap between name and desc

qreal padY()
{
    return Theme::compact() ? 4.0 : 7.0;
}

qreal nameLine()
{
    return Css::normalLine(Theme::Font::tweakName());
}

qreal descLine()
{
    return Css::line(Theme::Font::tweakDesc(), 1.45);
}

qreal textBlockHeight()
{
    return nameLine() + TextGap + descLine();
}

} // namespace

TweakRow::TweakRow(const Tweak &tweak, AppState *state, QWidget *parent)
    : QWidget(parent)
    , m_id(tweak.id)
    , m_name(tweak.name)
    , m_desc(tweak.desc)
    , m_state(state)
{
    setFixedHeight(rowHeight());

    m_toggle = new ToggleSwitch(this);
    m_toggle->setChecked(state->isOn(m_id), /*animate=*/false);
    connect(m_toggle, &ToggleSwitch::toggled, this, [this](bool on) {
        m_state->setOn(m_id, on);
    });
    // Keep the pill honest when something else moves the state (Geri al, Uygula).
    connect(state, &AppState::tweakToggled, this, [this](const QString &id) {
        if (id == m_id)
            m_toggle->setChecked(m_state->isOn(m_id));
    });

    positionToggle();
}

int TweakRow::rowHeight()
{
    return qRound(2 * (BorderW + padY()) + textBlockHeight());
}

QSize TweakRow::sizeHint() const
{
    return {0, rowHeight()};
}

void TweakRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    positionToggle();
}

void TweakRow::positionToggle()
{
    // `align-items:center` centres the 15px pill in the content box.
    const int x = qRound(BorderW + PadX);
    const int y = qRound((height() - Theme::Metric::ToggleHeight) / 2.0);
    m_toggle->move(x, y);
}

void TweakRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal top = BorderW + padY();
    const qreal textX = BorderW + PadX + ToggleCol + ColGap;
    const qreal textW = width() - textX - (BorderW + PadX);
    if (textW <= 0)
        return;

    const QFont &nameFont = Font::tweakName();
    const QFont &descFont = Font::tweakDesc();

    const QRectF box(textX, 0, textW, height());

    Css::drawText(&p, box, Css::baseline(nameFont, top, nameLine()),
                  nameFont, Color::TextPrimary(), m_name, Qt::AlignLeft, /*elide=*/true);

    const qreal descTop = top + nameLine() + TextGap;
    Css::drawText(&p, box, Css::baseline(descFont, descTop, descLine()),
                  descFont, Color::TextDesc(), m_desc, Qt::AlignLeft, /*elide=*/true);
}
