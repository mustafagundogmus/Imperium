#include "tweakpage.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../fluent/fluentgroupcard.h"
#include "../fluent/fluenttweakrow.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/sectionheader.h"
#include "../widgets/tweakrow.h"

#include <QLabel>
#include <QVBoxLayout>

namespace {
// The Fluent list: `padding:16px 36px 24px`, groups 20px apart, a 6px gap between a
// group's heading and its card. The 36 is the content column's 18 plus this page's 18.
constexpr int FluentPadX = 18;
constexpr int FluentPadTop = 16;
constexpr int FluentPadBottom = 24;
constexpr int FluentGroupGap = 20;
constexpr int FluentHeadGap = 6;
} // namespace

TweakPage::TweakPage(AppState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(0);
    outer->addStretch(1);
    m_layout = outer;

    setSections({});
}

void TweakPage::setSections(const QVector<Section> &sections, const QString &emptyMessage)
{
    // The whole list is swapped out as one widget: rebuilding in place would mean
    // deleting rows that can still be inside their own event handling.
    if (m_body) {
        m_layout->removeWidget(m_body);
        m_body->hide();
        m_body->deleteLater();
        m_body = nullptr;
    }

    const bool fluentShell = Theme::fluent();
    if (fluentShell)
        m_layout->setContentsMargins(FluentPadX, FluentPadTop, FluentPadX, FluentPadBottom);
    else
        m_layout->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                                     Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);

    m_body = new QWidget(this);
    auto *body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(fluentShell ? FluentGroupGap : Theme::Metric::SectionGap);

    int total = 0;
    for (const Section &s : sections)
        total += s.tweaks.size();

    if (fluentShell)
        buildFluent(body, sections);
    else
        buildClassic(body, sections);

    if (total == 0) {
        const QString message = emptyMessage.isEmpty() ? Locale::tr(QStringLiteral("tweak.noMatch"))
                                                       : emptyMessage;
        auto *empty = new QLabel(message, m_body);
        empty->setFont(Theme::Font::pageSub());
        QPalette pal = empty->palette();
        pal.setColor(QPalette::WindowText, Theme::Color::TextDim());
        empty->setPalette(pal);
        empty->setContentsMargins(6, 4, 0, 0);
        body->addWidget(empty);
    }

    m_layout->insertWidget(0, m_body);
    m_body->show();
}

void TweakPage::buildClassic(QVBoxLayout *body, const QVector<Section> &sections)
{
    for (const Section &s : sections) {
        if (s.tweaks.isEmpty())
            continue;

        auto *block = new QWidget(m_body);
        auto *blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(0, 0, 0, 0);
        blockLayout->setSpacing(0);

        auto *header = new SectionHeader(s.displayTitle(), block);
        header->setCount(Locale::tr(QStringLiteral("tweak.sectionCount")).arg(s.tweaks.size()));
        blockLayout->addWidget(header);

        auto *rows = new QWidget(block);
        auto *rowLayout = new QVBoxLayout(rows);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(1);
        for (const Tweak &t : s.tweaks)
            rowLayout->addWidget(new TweakRow(t, m_state, rows));
        blockLayout->addWidget(rows);

        body->addWidget(block);
    }
}

void TweakPage::buildFluent(QVBoxLayout *body, const QVector<Section> &sections)
{
    for (const Section &s : sections) {
        if (s.tweaks.isEmpty())
            continue;

        auto *block = new QWidget(m_body);
        auto *blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(0, 0, 0, 0);
        blockLayout->setSpacing(FluentHeadGap);

        blockLayout->addWidget(new FluentGroupHeader(
            s.displayTitle(), Locale::tr(QStringLiteral("fluent.groupCount")).arg(s.tweaks.size()),
            block));

        auto *card = new FluentGroupCard(block);
        for (const Tweak &t : s.tweaks)
            card->addRow(new FluentTweakRow(t, m_state, s.categoryId));
        blockLayout->addWidget(card);

        body->addWidget(block);
    }
}
