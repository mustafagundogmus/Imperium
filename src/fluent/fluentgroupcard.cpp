#include "fluentgroupcard.h"
#include "fluenttweakrow.h"
#include "../css.h"
#include "../theme.h"

#include <QPainter>
#include <QVBoxLayout>

namespace {
constexpr qreal HeadPadX = 4.0;
constexpr qreal HeadPadBottom = 4.0;
constexpr qreal HeadGap = 8.0;
constexpr qreal CardRadius = 6.0;
} // namespace

FluentGroupHeader::FluentGroupHeader(const QString &title, const QString &sub, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_sub(sub)
{
    setFixedHeight(sizeHint().height());
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedHeight(sizeHint().height());
        updateGeometry();
        update();
    });
}

QSize FluentGroupHeader::sizeHint() const
{
    return {0, qRound(Css::line(Theme::sans(13, Theme::Weight::SemiBold), 1.4) + HeadPadBottom)};
}

void FluentGroupHeader::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont titleFont = Theme::sans(13, Theme::Weight::SemiBold);
    const QFont subFont = Theme::sans(11);
    const qreal line = height() - HeadPadBottom;
    const qreal baseline = Css::baseline(titleFont, 0, line);   // `align-items:baseline`

    const qreal titleW = qMin(Css::textWidth(titleFont, m_title), width() - 2 * HeadPadX);
    Css::drawText(&p, QRectF(HeadPadX, 0, titleW, line), baseline, titleFont, t.text, m_title,
                  Qt::AlignLeft, true);
    if (!m_sub.isEmpty()) {
        const qreal x = HeadPadX + titleW + HeadGap;
        Css::drawText(&p, QRectF(x, 0, qMax(0.0, width() - HeadPadX - x), line), baseline, subFont,
                      t.textMuted, m_sub, Qt::AlignLeft, true);
    }
}

FluentGroupCard::FluentGroupCard(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    // One pixel inside the border, so a row's hover fill does not sit on the stroke.
    m_layout->setContentsMargins(1, 1, 1, 1);
    m_layout->setSpacing(0);
}

void FluentGroupCard::addRow(QWidget *row)
{
    row->setParent(this);
    m_layout->addWidget(row);
    m_rows.append(row);
    for (int i = 0; i < m_rows.size(); ++i)
        if (auto *r = qobject_cast<FluentTweakRow *>(m_rows.at(i)))
            r->setEdges(i == 0, i == m_rows.size() - 1);
}

void FluentGroupCard::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setPen(QPen(t.cardBorder, 1.0));
    p.setBrush(t.card);
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), CardRadius, CardRadius);

    for (int i = 1; i < m_rows.size(); ++i)
        Css::hairline(&p, QRectF(1, m_rows.at(i)->y(), width() - 2, 1), t.divider);
}
