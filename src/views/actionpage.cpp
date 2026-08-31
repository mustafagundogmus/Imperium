#include "actionpage.h"

#include "../action.h"
#include "../actionengine.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/buttons.h"
#include "../widgets/sectionheader.h"
#include "../widgets/settingrow.h"

#include <QCoreApplication>
#include "../widgets/dialog.h"
#include <QPushButton>
#include <QVBoxLayout>

namespace {

} // namespace

ActionPage::ActionPage(QWidget *parent)
    : QWidget(parent)
    , m_engine(new ActionEngine(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                              Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    outer->setSpacing(Theme::Metric::SectionGap);

    for (const ActionSection &section : ActionCatalog::instance().sections())
        outer->addWidget(buildSection(section));
    outer->addStretch(1);

    connect(m_engine, &ActionEngine::started, this, [this](const QString &id) {
        if (PillButton *button = m_buttons.value(id))
            button->setEnabledLook(false);
        if (SettingRow *row = m_rows.value(id))
            row->setDesc(Locale::tr(QStringLiteral("actions.running")));
    });

    connect(m_engine, &ActionEngine::finished, this,
            [this](const QString &id, bool ok, const QString &output) {
                if (PillButton *button = m_buttons.value(id))
                    button->setEnabledLook(true);

                // The script's last line is the summary it prints for itself; everything
                // it said is in actions.log, which the row points at when a run failed.
                const QStringList spoken = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                QString summary = spoken.isEmpty()
                                      ? (ok ? Locale::tr(QStringLiteral("actions.status.done"))
                                            : Locale::tr(QStringLiteral("actions.status.failed")))
                                      : spoken.last().trimmed();

                // A script cannot know what language the interface is in, so it reports its
                // outcome as a token — "ARB|<code>" with an optional argument — and the
                // sentence is built here. The raw token is still what goes to actions.log,
                // which is a diagnostic file and is better off language-independent.
                if (summary.startsWith(QLatin1String("ARB|"))) {
                    const QStringList parts = summary.split(QLatin1Char('|'));
                    const QString key = QStringLiteral("actions.result.") + parts.value(1);
                    summary = parts.size() > 2 ? Locale::tr(key).arg(parts.value(2))
                                               : Locale::tr(key);
                }

                if (SettingRow *row = m_rows.value(id))
                    row->setDesc(ok ? summary
                                    : Locale::tr(QStringLiteral("actions.detail")).arg(summary));
                Q_EMIT notice(ok ? summary
                                 : Locale::tr(QStringLiteral("actions.notice.failed")).arg(summary));
            });

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &ActionPage::retranslate);
}

void ActionPage::retranslate()
{
    const QVector<ActionSection> &sections = ActionCatalog::instance().sections();
    for (int i = 0; i < m_sectionHeaders.size() && i < sections.size(); ++i) {
        m_sectionHeaders.at(i)->setTitle(sections.at(i).displayTitle());
        m_sectionHeaders.at(i)->setCount(
            Locale::tr(QStringLiteral("actions.count")).arg(sections.at(i).actions.size()));
    }

    for (const ActionSection &section : sections) {
        for (const Action &action : section.actions) {
            const QString desc = action.reversible
                                     ? action.displayDesc()
                                     : Locale::tr(QStringLiteral("actions.irreversible"))
                                           .arg(action.displayDesc());
            if (SettingRow *row = m_rows.value(action.id)) {
                row->setName(action.displayName());
                row->setDesc(desc);
            }
        }
    }

    for (PillButton *button : std::as_const(m_buttons))
        button->setText(Locale::tr(QStringLiteral("actions.run")));
}

QWidget *ActionPage::buildSection(const ActionSection &section)
{
    auto *block = new QWidget(this);
    auto *layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new SectionHeader(section.displayTitle(), block);
    header->setCount(Locale::tr(QStringLiteral("actions.count")).arg(section.actions.size()));
    layout->addWidget(header);
    m_sectionHeaders.append(header);

    auto *list = new QWidget(block);
    auto *listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(1);

    for (const Action &action : section.actions) {
        auto *button = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("actions.run")));
        connect(button, &PillButton::clicked, this, [this, action] { confirmAndRun(action); });

        // The description says up front whether this can be walked back, because that is
        // the one thing that separates these rows from every other row in the app.
        const QString desc = action.reversible
                                 ? action.displayDesc()
                                 : Locale::tr(QStringLiteral("actions.irreversible"))
                                       .arg(action.displayDesc());

        auto *row = new SettingRow(action.displayName(), desc, button, SettingRow::Trailing, list);
        listLayout->addWidget(row);

        m_buttons.insert(action.id, button);
        m_rows.insert(action.id, row);
        ++m_rowCount;
    }

    layout->addWidget(list);
    return block;
}

void ActionPage::confirmAndRun(const Action &action)
{
    if (m_engine->running()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("actions.notice.busy")));
        return;
    }

    QString informative = action.displayDesc();
    informative += action.reversible
                       ? Locale::tr(QStringLiteral("actions.confirm.reversible"))
                       : Locale::tr(QStringLiteral("actions.confirm.irreversible"));
    if (!action.note.isEmpty())
        informative += QStringLiteral("\n") + action.displayNote();
    // The last argument is not a formality: it is the whole script, so a user who wants
    // to know exactly what is about to run can read it here instead of trusting the
    // description. The dialog puts it behind a toggle and lets it be selected and copied.
    if (Dialog::confirm(this, action.displayName(), informative,
                        Locale::tr(QStringLiteral("actions.run")),
                        Locale::tr(QStringLiteral("actions.cancel")),
                        action.script()))
        m_engine->run(action);
}
