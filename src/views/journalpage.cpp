#include "journalpage.h"

#include "../appstate.h"
#include "../catalog.h"
#include "../i18n.h"
#include "../registry.h"
#include "../sysinfo.h"
#include "../theme.h"
#include "../widgets/buttons.h"
#include "../widgets/sectionheader.h"
#include "../widgets/settingrow.h"

#include <QLabel>
#include <QVBoxLayout>

namespace {

/// The journal grows without bound; the screen does not need to.
constexpr int MaxRows = 300;

/// "HKCU\Software\…\Enabled · 1 → 0"
QString target(const TweakEngine::JournalEntry &entry)
{
    const QString value = entry.value.isEmpty() ? Locale::tr(QStringLiteral("journal.value.default"))
                                                : entry.value;
    const QString before = entry.existed ? entry.previousData
                                         : Locale::tr(QStringLiteral("journal.value.absent"));
    // Spelled through the sentinels rather than as literals: they are defined once in
    // registry.h and every other comparison in the app goes through them.
    const QString after =
        entry.desired.compare(Registry::DeleteSentinel, Qt::CaseInsensitive) == 0
            ? Locale::tr(QStringLiteral("journal.value.deleted"))
            : entry.desired.compare(Registry::DeleteKeySentinel, Qt::CaseInsensitive) == 0
                  ? Locale::tr(QStringLiteral("journal.value.keyDeleted"))
                  : entry.desired;

    const QString empty = Locale::tr(QStringLiteral("journal.value.empty"));
    return QStringLiteral("%1\\%2\\%3 · %4 → %5")
        .arg(entry.hive, entry.path, value, before.isEmpty() ? empty : before,
             after.isEmpty() ? empty : after);
}

} // namespace

JournalPage::JournalPage(TweakEngine *engine, AppState *state, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_state(state)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                                 Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    m_layout->setSpacing(Theme::Metric::SectionGap);
    m_layout->addStretch(1);

    reload();
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &JournalPage::reload);
}

void JournalPage::reload()
{
    // Swapped out as one widget rather than rebuilt in place: a row can be deleted while
    // its own button is still inside the click that asked for it.
    if (m_body) {
        m_layout->removeWidget(m_body);
        m_body->hide();
        m_body->deleteLater();
        m_body = nullptr;
    }
    m_rows.clear();
    m_buttons.clear();

    m_entries = m_engine->history(MaxRows);
    m_rowCount = int(m_entries.size());

    m_body = new QWidget(this);
    auto *body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    if (m_entries.isEmpty()) {
        auto *empty = new QLabel(Locale::tr(QStringLiteral("journal.empty.body")), m_body);
        empty->setFont(Theme::Font::pageSub());
        QPalette pal = empty->palette();
        pal.setColor(QPalette::WindowText, Theme::Color::TextFaint());
        empty->setPalette(pal);
        body->addWidget(empty);
        m_layout->insertWidget(0, m_body);
        return;
    }

    auto *header = new SectionHeader(Locale::tr(QStringLiteral("journal.header")), m_body);
    header->setCount(Locale::tr(QStringLiteral("journal.count")).arg(m_entries.size()));
    body->addWidget(header);

    auto *list = new QWidget(m_body);
    auto *listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(1);

    for (int i = 0; i < m_entries.size(); ++i) {
        const TweakEngine::JournalEntry &entry = m_entries.at(i);

        auto *button = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("journal.revert")));

        // A line that recorded a whole-key deletion describes one value out of everything
        // that went with the key, so there is no honest undo to offer. Dimmed rather than
        // hidden: the write still happened and the log's job is to say so.
        const bool keyDelete = entry.desired.compare(Registry::DeleteKeySentinel,
                                                     Qt::CaseInsensitive) == 0;
        if (keyDelete)
            button->setEnabledLook(false);
        else
            connect(button, &PillButton::clicked, this, [this, i] { revert(i); });

        // The journal stores the name as it read at the time of the write, which is the
        // language that was in use then. Looking the id back up in the catalogue is what
        // lets an old entry be read in the language that is in use now; the stored name
        // stays as the fallback for a tweak the catalogue no longer carries.
        QString shown = entry.tweakName.isEmpty() ? entry.tweakId : entry.tweakName;
        if (const Tweak *t = Catalog::instance().tweak(entry.tweakId))
            shown = t->displayName();

        auto *row = new SettingRow(
            QStringLiteral("%1 · %2").arg(shown, SysInfo::friendlyDateTime(entry.at)),
            target(entry), button, SettingRow::Trailing, list);
        listLayout->addWidget(row);

        m_rows.insert(i, row);
        m_buttons.insert(i, button);
    }

    body->addWidget(list);
    m_layout->insertWidget(0, m_body);
}

void JournalPage::revert(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;

    const TweakEngine::JournalEntry entry = m_entries.at(index);

    QString error;
    if (!m_engine->revert(entry, &error)) {
        if (SettingRow *row = m_rows.value(index))
            row->setDesc(Locale::tr(QStringLiteral("journal.revertFailed")).arg(error));
        Q_EMIT notice(Locale::tr(QStringLiteral("journal.revertFailed")).arg(error));
        return;
    }

    // The switch for this tweak is now looking at a value nobody told it about.
    m_state->refreshFromMachine(entry.tweakId);

    if (SettingRow *row = m_rows.value(index))
        row->setDesc(Locale::tr(QStringLiteral("journal.reverted")).arg(entry.hive, entry.path));
    if (PillButton *button = m_buttons.value(index))
        button->setEnabledLook(false);

    QString shown = entry.tweakName.isEmpty() ? entry.tweakId : entry.tweakName;
    if (const Tweak *t = Catalog::instance().tweak(entry.tweakId))
        shown = t->displayName();
    Q_EMIT notice(Locale::tr(QStringLiteral("journal.revertedNotice")).arg(shown));
}
