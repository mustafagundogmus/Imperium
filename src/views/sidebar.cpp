#include "sidebar.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/categoryrow.h"
#include "../widgets/searchfield.h"
#include "../widgets/sectionheader.h"
#include "../widgets/smoothscrollarea.h"

#include <QPainter>

namespace {

constexpr qreal PadX = 8.0;          // sidebar padding
constexpr qreal PadTop = 10.0;
constexpr qreal SearchInset = 2.0;   // the search box's own 2px side margin
constexpr qreal SearchGapBelow = 10.0;
constexpr int RowGap = 1;
constexpr qreal GroupGapAbove = 12.0;   // extra air before a group header (beyond RowGap)

constexpr qreal BlockPadBottom = 12.0;   // bottom block padding

// Pinned below the divider: settings are not a tweak category, so they get the list's row
// shape but sit outside the scrolling group. The restore point used to live under this
// row; it is a setting, and it now says so — see SettingsPage's Güvenlik section.
constexpr qreal SettingsGap = 6.0;
constexpr int PinnedRows = 5;   // Günlük + Eylemler + TrustedInstaller + Ayarlar + Hakkında

const QString JournalIcon = QStringLiteral(
    "M2.5 1.5h7v9h-7zM4.5 4h3M4.5 6h3M4.5 8h2");
const QString ActionsIcon = QStringLiteral(
    "M2 6h8M6 2v8M3.6 3.6l4.8 4.8M8.4 3.6L3.6 8.4");
// A shield with a check: the authority mark. TrustedInstaller is the account that owns
// what an administrator is refused, so a shield reads truer here than a key would.
const QString TiLauncherIcon = QStringLiteral(
    "M6 1.4L9.8 2.9v3.2c0 2.4-1.6 4-3.8 4.9-2.2-.9-3.8-2.5-3.8-4.9V2.9z"
    "M4.4 6.1l1.1 1.1 2.2-2.6");
// A trash can: lid + tapered body + two ribs. Debloat removes installed packages, which
// is closer to Eylemler's one-shot actions than to a reversible tweak position.
const QString DebloatIcon = QStringLiteral(
    "M2.3 3.4h7.4M4.3 3.4v-1a.7.7 0 01.7-.7h2a.7.7 0 01.7.7v1"
    "M3 3.4l.5 6.3a.9.9 0 00.9.8h3.2a.9.9 0 00.9-.8l.5-6.3"
    "M5 5.1v3.4M7 5.1v3.4");
// A cog: a toothed ring around a hub. What was here before was a disc with eight detached
// rays — the same drawing as the Görsel Efektler category, i.e. a brightness icon.
const QString SettingsIcon = QStringLiteral(
    "M6 2.7a3.3 3.3 0 100 6.6 3.3 3.3 0 000-6.6"
    "M6 4.8a1.2 1.2 0 100 2.4 1.2 1.2 0 000-2.4"
    "M9.3 6H11M2.7 6H1"
    "M7.65 8.86l.85 1.47M4.35 8.86l-.85 1.47"
    "M4.35 3.14l-.85-1.47M7.65 3.14l.85-1.47");
// An "i" in a circle — the conventional about/info mark. The dot is a near-zero-length
// segment; drawn with round caps (like every other glyph in this set) it reads as one.
const QString AboutIcon = QStringLiteral(
    "M6 1.5a4.5 4.5 0 100 9 4.5 4.5 0 000-9"
    "M6 5.4v2.6M6 3.7v.1");

struct GroupDef
{
    const char *labelKey;   ///< i18n key, or nullptr for the ungrouped row (Genel Bakış)
    const char *ids[8];     ///< category ids, empty-string terminated
};

// Reuses whichever i18n key already carries the right word rather than adding a
// near-duplicate: "Görünüm" is already settings.section.appearance, "Sistem" is already
// category.sys, "Gelişmiş" is already category.adv. Only the two combined labels are new.
//
// "debloat" rides along in the Sistem group even though it is not a real catalogue
// category (buildList() special-cases that one id) — it is a system-management page in
// exactly the sense Sistem/Hizmetler/Başlangıç are, so it reads out of place anywhere
// else, and it read especially oddly sitting in the pinned utility strip at the bottom
// next to Günlük/Ayarlar/Hakkında, which are meta pages rather than things to act on.
constexpr GroupDef Groups[] = {
    { nullptr,                        {"ov", "", "", "", "", "", "", ""} },
    { "settings.section.appearance",  {"vis", "", "", "", "", "", "", ""} },
    // Windows Update sits next to Sistem because that is what it is; Güç yönetimi next
    // to Bellek & CPU because the two are read together.
    { "category.sys",                 {"sys", "upd", "svc", "boot", "perf", "pwr", "debloat", ""} },
    // Güvenlik sertleştirme belongs beside Gizlilik: the same page of a user's mind.
    { "sidebar.group.privacynet",     {"priv", "sec", "net", "", "", "", "", ""} },
    { "sidebar.group.files",          {"exp", "ctx", "cln", "", "", "", "", ""} },
    { "category.adv",                 {"adv", "", "", "", "", "", "", ""} },
};

} // namespace

Sidebar::Sidebar(AppState *state, QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(Theme::Metric::SidebarWidth);

    m_search = new SearchField(this);

    // Inside a scroll area rather than laid straight on the sidebar. The list was a plain
    // widget clipped to whatever height was left over, so a row past the bottom edge did
    // not scroll into view — it just was not there. Three more categories would have taken
    // Gelişmiş off the end of it, and a large interface scale did the same thing already.
    m_listScroll = new SmoothScrollArea(this);
    m_listScroll->setFrameShape(QFrame::NoFrame);
    m_listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listScroll->viewport()->setAutoFillBackground(false);

    m_list = new QWidget(m_listScroll);
    m_list->setAutoFillBackground(false);
    buildList();
    m_listScroll->setWidget(m_list);

    m_journal = new CategoryRow(journalId(), Locale::tr(QStringLiteral("sidebar.journal")), JournalIcon,
                                QString(), this);
    connect(m_journal, &CategoryRow::activated, this, &Sidebar::categoryActivated);

    m_actions = new CategoryRow(actionsId(), Locale::tr(QStringLiteral("sidebar.actions")), ActionsIcon,
                                QString(), this);
    connect(m_actions, &CategoryRow::activated, this, &Sidebar::categoryActivated);

    m_tiLauncher = new CategoryRow(tiLauncherId(), Locale::tr(QStringLiteral("sidebar.tilauncher")),
                                   TiLauncherIcon, QString(), this);
    connect(m_tiLauncher, &CategoryRow::activated, this, &Sidebar::categoryActivated);

    m_settings = new CategoryRow(settingsId(), Locale::tr(QStringLiteral("sidebar.settings")), SettingsIcon,
                                 QString(), this);
    connect(m_settings, &CategoryRow::activated, this, &Sidebar::categoryActivated);

    m_about = new CategoryRow(aboutId(), Locale::tr(QStringLiteral("sidebar.about")), AboutIcon,
                              QString(), this);
    connect(m_about, &CategoryRow::activated, this, &Sidebar::categoryActivated);

    setSelected(state->selectedCategory());

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        for (CategoryRow *row : std::as_const(m_rows)) {
            row->setName(row->categoryId() == debloatId()
                             ? Locale::tr(QStringLiteral("sidebar.debloat"))
                             : Locale::tr(QStringLiteral("category.") + row->categoryId()));
        }
        retranslateGroups();
        m_journal->setName(Locale::tr(QStringLiteral("sidebar.journal")));
        m_actions->setName(Locale::tr(QStringLiteral("sidebar.actions")));
        m_tiLauncher->setName(Locale::tr(QStringLiteral("sidebar.tilauncher")));
        m_settings->setName(Locale::tr(QStringLiteral("sidebar.settings")));
        m_about->setName(Locale::tr(QStringLiteral("sidebar.about")));
    });

    // The search field's own height grows with the interface scale (see SearchField);
    // everything below it has to slide down to match, which means a full relayout.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this,
           [this] { resizeEvent(nullptr); });
}

void Sidebar::buildList()
{
    const Catalog &catalog = Catalog::instance();

    for (const GroupDef &group : Groups) {
        SectionHeader *header = nullptr;
        if (group.labelKey) {
            header = new SectionHeader(Locale::tr(QString::fromLatin1(group.labelKey)), m_list);
            m_groupHeaders.append({header, QString::fromLatin1(group.labelKey)});
            m_listItems.append(header);
        }

        for (const char *rawId : group.ids) {
            if (!*rawId)
                break;
            const QString id = QString::fromLatin1(rawId);

            CategoryRow *row = nullptr;
            if (id == debloatId()) {
                // Not a catalogue category — a live machine scan behind the same row
                // shape, so it reads as part of the list instead of a special case.
                row = new CategoryRow(id, Locale::tr(QStringLiteral("sidebar.debloat")),
                                      DebloatIcon, QString(), m_list);
            } else if (const Category *category = catalog.category(id)) {
                const int count = category->tweakCount();
                row = new CategoryRow(category->id,
                                      Locale::tr(QStringLiteral("category.") + category->id),
                                      category->icon,
                                      count > 0 ? QString::number(count) : QString(), m_list);
            } else {
                continue;   // a build that trims a category should not crash over it
            }

            connect(row, &CategoryRow::activated, this, &Sidebar::categoryActivated);
            m_rows.append(row);
            m_listItems.append(row);
        }
    }
}

void Sidebar::setCategoryCount(const QString &categoryId, const QString &text)
{
    for (CategoryRow *row : std::as_const(m_rows)) {
        if (row->categoryId() == categoryId) {
            row->setCount(text);
            return;
        }
    }
}

void Sidebar::retranslateGroups()
{
    for (const auto &entry : std::as_const(m_groupHeaders))
        entry.first->setTitle(Locale::tr(entry.second));
}

void Sidebar::setSelected(const QString &categoryId)
{
    for (CategoryRow *row : std::as_const(m_rows))
        row->setSelected(row->categoryId() == categoryId);
    m_settings->setSelected(categoryId == settingsId());
    m_actions->setSelected(categoryId == actionsId());
    m_tiLauncher->setSelected(categoryId == tiLauncherId());
    m_journal->setSelected(categoryId == journalId());
    m_about->setSelected(categoryId == aboutId());
}

qreal Sidebar::aboutRowTop() const
{
    return height() - BlockPadBottom - Theme::Metric::CategoryHeight;
}

qreal Sidebar::settingsRowTop() const
{
    return aboutRowTop() - RowGap - Theme::Metric::CategoryHeight;
}

qreal Sidebar::tiLauncherRowTop() const
{
    return settingsRowTop() - RowGap - Theme::Metric::CategoryHeight;
}

qreal Sidebar::actionsRowTop() const
{
    return tiLauncherRowTop() - RowGap - Theme::Metric::CategoryHeight;
}

qreal Sidebar::journalRowTop() const
{
    return actionsRowTop() - RowGap - Theme::Metric::CategoryHeight;
}

int Sidebar::bottomBlockHeight() const
{
    return qRound(1.0 + SettingsGap
                  + PinnedRows * Theme::Metric::CategoryHeight + (PinnedRows - 1) * RowGap
                  + BlockPadBottom);
}

void Sidebar::resizeEvent(QResizeEvent *e)
{
    if (e)
        QWidget::resizeEvent(e);

    const int contentW = qRound(width() - 1 /*right border*/ - 2 * PadX);

    // Height comes from the field itself rather than the metric constant, so a larger
    // interface scale pushes the list down instead of the field's text overflowing it.
    const int searchH = m_search->sizeHint().height();
    m_search->setGeometry(qRound(PadX + SearchInset), qRound(PadTop),
                          qRound(contentW - 2 * SearchInset), searchH);

    const int listTop = qRound(PadTop + searchH + SearchGapBelow);
    const int blockH = bottomBlockHeight();
    const int listH = qMax(0, height() - blockH - listTop);
    m_listScroll->setGeometry(qRound(PadX), listTop, contentW, listH);

    // Group headers get extra air above them; an ordinary row just follows the last
    // widget by RowGap, whatever it was. Returns the height the stack came to.
    const auto layoutRows = [this](int rowW) {
        int y = 0;
        bool first = true;
        for (QWidget *item : std::as_const(m_listItems)) {
            const bool isHeader = qobject_cast<SectionHeader *>(item) != nullptr;
            if (!first)
                y += isHeader ? qRound(GroupGapAbove) : RowGap;
            first = false;

            const int h = isHeader ? item->sizeHint().height() : Theme::Metric::CategoryHeight;
            item->setGeometry(0, y, rowW, h);
            y += h;
        }
        return y;
    };

    // Measured at full width first, and laid out again 8px narrower only if the result
    // does not fit — asking the viewport for its width instead would answer with whatever
    // it was before this geometry was set, which on the first pass is nothing like right.
    int rowW = contentW;
    int listContentH = layoutRows(rowW);
    if (listContentH > listH) {
        rowW = qMax(0, contentW - Theme::Metric::ScrollBarWidth);
        listContentH = layoutRows(rowW);
    }
    m_list->setFixedSize(rowW, listContentH);

    m_journal->setGeometry(qRound(PadX), qRound(journalRowTop()),
                           contentW, Theme::Metric::CategoryHeight);
    m_actions->setGeometry(qRound(PadX), qRound(actionsRowTop()),
                           contentW, Theme::Metric::CategoryHeight);
    m_tiLauncher->setGeometry(qRound(PadX), qRound(tiLauncherRowTop()),
                              contentW, Theme::Metric::CategoryHeight);
    m_settings->setGeometry(qRound(PadX), qRound(settingsRowTop()),
                            contentW, Theme::Metric::CategoryHeight);
    m_about->setGeometry(qRound(PadX), qRound(aboutRowTop()),
                         contentW, Theme::Metric::CategoryHeight);
}

void Sidebar::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    Css::hairline(&p, QRectF(width() - 1, 0, 1, height()), Color::Divider());

    const qreal contentLeft = PadX;
    const qreal contentRight = width() - 1 - PadX;

    const qreal borderY = std::round(journalRowTop() - SettingsGap - 1.0);
    Css::hairline(&p, QRectF(contentLeft, borderY, contentRight - contentLeft, 1),
                  Color::Divider());
}
