#include "sidebar.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../css.h"
#include "../theme.h"
#include "../widgets/buttons.h"
#include "../widgets/categoryrow.h"
#include "../widgets/searchfield.h"

#include <QPainter>

namespace {

constexpr qreal PadX = 8.0;          // sidebar padding
constexpr qreal PadTop = 10.0;
constexpr qreal SearchInset = 2.0;   // the search box's own 2px side margin
constexpr qreal SearchGapBelow = 10.0;
constexpr int RowGap = 1;

constexpr qreal BlockPadX = 8.0;     // bottom block padding 10px 8px 12px
constexpr qreal BlockPadTop = 10.0;
constexpr qreal BlockPadBottom = 12.0;
constexpr qreal BlockGap = 3.0;

const QString RestoreLabel = QStringLiteral("Geri yükleme noktası");

// Pinned below the divider, above the restore block: settings are not a tweak category,
// so they get the list's row shape but sit outside the scrolling group.
constexpr qreal SettingsGap = 6.0;
const QString SettingsIcon = QStringLiteral(
    "M6 4.3a1.7 1.7 0 100 3.4 1.7 1.7 0 000-3.4M6 1v1.5M6 9.5V11M1 6h1.5M9.5 6H11"
    "M2.6 2.6l1.1 1.1M8.3 8.3l1.1 1.1M9.4 2.6L8.3 3.7M3.7 8.3L2.6 9.4");

} // namespace

Sidebar::Sidebar(AppState *state, QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(Theme::Metric::SidebarWidth);

    m_search = new SearchField(this);

    m_list = new QWidget(this);
    for (const Category &c : Catalog::instance().categories()) {
        const int count = c.tweakCount();
        auto *row = new CategoryRow(c.id, c.name, c.icon,
                                    count > 0 ? QString::number(count) : QString(),
                                    m_list);
        connect(row, &CategoryRow::activated, this, &Sidebar::categoryActivated);
        m_rows.append(row);
    }

    m_settings = new CategoryRow(settingsId(), QStringLiteral("Ayarlar"), SettingsIcon,
                                 QString(), this);
    connect(m_settings, &CategoryRow::activated, this, &Sidebar::categoryActivated);

    m_link = new LinkLabel(QStringLiteral("Yeni oluştur"), this);
    connect(m_link, &LinkLabel::clicked, this, &Sidebar::restorePointRequested);

    setSelected(state->selectedCategory());
}

void Sidebar::setSelected(const QString &categoryId)
{
    for (CategoryRow *row : std::as_const(m_rows))
        row->setSelected(row->categoryId() == categoryId);
    m_settings->setSelected(categoryId == settingsId());
}

void Sidebar::setRestorePoint(const QString &value)
{
    if (m_restorePoint == value)
        return;
    m_restorePoint = value;
    update();
}

// The value/link row is baseline aligned, so its height is the deepest ascent plus the
// deepest descent — not the taller of the two line boxes. Every other measurement in the
// bottom block is derived from this one so the paint and the layout can never drift.

qreal Sidebar::restoreRowAscent()
{
    return qMax(Css::ascent(Theme::Font::restoreValue()), Css::ascent(Theme::Font::link()));
}

qreal Sidebar::restoreRowHeight()
{
    return restoreRowAscent()
           + qMax(Css::descent(Theme::Font::restoreValue()), Css::descent(Theme::Font::link()));
}

qreal Sidebar::restoreRowTop() const
{
    return height() - BlockPadBottom - restoreRowHeight();
}

qreal Sidebar::settingsRowTop() const
{
    return restoreLabelTop() - BlockPadTop - SettingsGap - Theme::Metric::CategoryHeight;
}

qreal Sidebar::restoreLabelTop() const
{
    return restoreRowTop() - BlockGap - Css::normalLine(Theme::Font::upperLabel());
}

int Sidebar::bottomBlockHeight() const
{
    return qRound(1.0 + SettingsGap + Theme::Metric::CategoryHeight + SettingsGap
                  + BlockPadTop + Css::normalLine(Theme::Font::upperLabel())
                  + BlockGap + restoreRowHeight() + BlockPadBottom);
}

void Sidebar::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    const int contentW = qRound(width() - 1 /*right border*/ - 2 * PadX);

    m_search->setGeometry(qRound(PadX + SearchInset), qRound(PadTop),
                          qRound(contentW - 2 * SearchInset), Theme::Metric::SearchHeight);

    const int listTop = qRound(PadTop + Theme::Metric::SearchHeight + SearchGapBelow);
    const int blockH = bottomBlockHeight();
    const int listH = qMax(0, height() - blockH - listTop);
    m_list->setGeometry(qRound(PadX), listTop, contentW, listH);

    int y = 0;
    for (CategoryRow *row : std::as_const(m_rows)) {
        row->setGeometry(0, y, contentW, Theme::Metric::CategoryHeight);
        y += Theme::Metric::CategoryHeight + RowGap;
    }

    m_settings->setGeometry(qRound(PadX), qRound(settingsRowTop()),
                            contentW, Theme::Metric::CategoryHeight);

    // The link sits at the right of the value row, sharing its baseline.
    const qreal linkTop = restoreRowTop() + restoreRowAscent() - Css::ascent(Theme::Font::link());
    m_link->move(qRound(width() - 1 - PadX - BlockPadX - m_link->width()), qRound(linkTop));
}

void Sidebar::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    Css::hairline(&p, QRectF(width() - 1, 0, 1, height()), Color::Divider());

    const qreal contentLeft = PadX;
    const qreal contentRight = width() - 1 - PadX;

    const qreal labelTop = restoreLabelTop();
    const qreal borderY = std::round(settingsRowTop() - SettingsGap - 1.0);
    Css::hairline(&p, QRectF(contentLeft, borderY, contentRight - contentLeft, 1),
                  Color::Divider());

    const QFont &labelFont = Font::upperLabel();
    const QRectF inner(contentLeft + BlockPadX, 0,
                       (contentRight - BlockPadX) - (contentLeft + BlockPadX), height());

    Css::drawText(&p, inner, Css::baseline(labelFont, labelTop, Css::normalLine(labelFont)),
                  labelFont, Color::TextFaint(), Css::upperTr(RestoreLabel));

    // Leave room for the link so a long timestamp is elided instead of colliding.
    const QRectF valueBox(inner.left(), 0,
                          qMax(0.0, inner.width() - m_link->width() - 8.0), height());
    Css::drawText(&p, valueBox, restoreRowTop() + restoreRowAscent(), Font::restoreValue(),
                  Color::TextMono(), m_restorePoint, Qt::AlignLeft, true);
}
