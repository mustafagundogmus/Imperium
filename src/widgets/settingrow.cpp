#include "settingrow.h"

#include "../css.h"
#include "../theme.h"

#include <QPainter>

namespace {

constexpr qreal BorderW = 1.0;
constexpr qreal PadX = 6.0;
const qreal LeadingCol = Theme::Metric::ToggleWidth;
constexpr qreal ColGap = 12.0;
constexpr qreal TrailingGap = 16.0;
constexpr qreal TextGap = 1.0;

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

} // namespace

SettingRow::SettingRow(const QString &name, const QString &desc, QWidget *control,
                       Placement placement, QWidget *parent)
    : QWidget(parent)
    , m_name(name)
    , m_desc(desc)
    , m_control(control)
    , m_placement(placement)
{
    if (m_control)
        m_control->setParent(this);
    setFixedHeight(sizeHint().height());
    positionControl();
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

int SettingRow::rowHeight()
{
    return qRound(2 * (BorderW + padY()) + nameLine() + TextGap + descLine());
}

QSize SettingRow::sizeHint() const
{
    int h = rowHeight();
    if (m_control)
        h = qMax(h, int(m_control->sizeHint().height() + 2 * (BorderW + padY())));
    return {0, h};
}

void SettingRow::setDesc(const QString &desc)
{
    if (m_desc == desc)
        return;
    m_desc = desc;
    update();
}

void SettingRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    positionControl();
}

void SettingRow::positionControl()
{
    if (!m_control)
        return;

    const QSize hint = m_control->sizeHint();
    m_control->resize(hint.isEmpty() ? m_control->size() : hint);

    const int y = qRound((height() - m_control->height()) / 2.0);
    if (m_placement == Leading)
        m_control->move(qRound(BorderW + PadX) - Theme::Metric::ToggleBleed, y);
    else
        m_control->move(qRound(width() - BorderW - PadX - m_control->width()), y);
}

void SettingRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal textX = m_placement == Leading ? BorderW + PadX + LeadingCol + ColGap
                                               : BorderW + PadX;
    qreal textRight = width() - (BorderW + PadX);
    if (m_placement == Trailing && m_control)
        textRight = m_control->x() - TrailingGap;

    const qreal textW = textRight - textX;
    if (textW <= 0)
        return;

    // The text block is centred on the row so a taller control does not drag it upwards.
    const qreal block = nameLine() + TextGap + descLine();
    const qreal top = (height() - block) / 2.0;

    const QFont &nameFont = Font::tweakName();
    const QFont &descFont = Font::tweakDesc();
    const QRectF box(textX, 0, textW, height());

    Css::drawText(&p, box, Css::baseline(nameFont, top, nameLine()),
                  nameFont, Color::TextPrimary(), m_name, Qt::AlignLeft, true);
    Css::drawText(&p, box, Css::baseline(descFont, top + nameLine() + TextGap, descLine()),
                  descFont, Color::TextDesc(), m_desc, Qt::AlignLeft, true);
}
