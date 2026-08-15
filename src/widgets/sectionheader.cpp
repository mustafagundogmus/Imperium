#include "sectionheader.h"
#include "../css.h"
#include "../theme.h"

#include <QPainter>

namespace {
constexpr qreal PadX = 6.0;
constexpr qreal PadBottom = 6.0;
constexpr qreal Gap = 10.0;
} // namespace

SectionHeader::SectionHeader(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(Css::upperTr(title))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // The row has one intrinsic height and always spans the full column width; saying so
    // explicitly keeps a QVBoxLayout from collapsing it to nothing.
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
}

void SectionHeader::setCount(const QString &text)
{
    if (m_count == text)
        return;
    m_count = text;
    update();
}

QSize SectionHeader::sizeHint() const
{
    // The flex row is as tall as its tallest child — the label's line box.
    const qreal labelLine = Css::normalLine(Theme::Font::sectionTitle());
    return {0, qRound(labelLine + PadBottom)};
}

void SectionHeader::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont &labelFont = Font::sectionTitle();
    const qreal labelLine = Css::normalLine(labelFont);
    const QRectF content(PadX, 0, width() - 2 * PadX, labelLine);

    const qreal labelW = Css::textWidth(labelFont, m_title);
    Css::drawText(&p, content, Css::baseline(labelFont, 0, labelLine),
                  labelFont, Color::TextDim(), m_title);

    qreal countW = 0;
    if (!m_count.isEmpty()) {
        const QFont &countFont = Theme::Font::sectionCount();
        countW = Css::textWidth(countFont, m_count);
        Css::drawText(&p, content, Css::centeredBaseline(countFont, content),
                      countFont, Color::TextFainter(), m_count, Qt::AlignRight);
    }

    // The 1px rule stretches between them, centred in the row.
    const qreal ruleLeft = content.left() + labelW + Gap;
    const qreal ruleRight = content.right() - (countW > 0 ? countW + Gap : 0.0);
    if (ruleRight > ruleLeft) {
        const qreal y = std::round((labelLine - 1.0) / 2.0);
        Css::hairline(&p, QRectF(ruleLeft, y, ruleRight - ruleLeft, 1.0), Color::Divider());
    }
}
