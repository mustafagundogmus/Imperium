#include "debloatpage.h"

#include "../action.h"
#include "../actionengine.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/buttons.h"
#include "../widgets/debloatrow.h"
#include "../widgets/sectionheader.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int TopBarGap = 10;

/// Section titles, indexed by InstalledApp::section().
constexpr const char *SectionKeys[] = {
    "debloat.section.apps",
    "debloat.section.components",
    "debloat.section.staged",
};
constexpr int SectionCount = 3;

/// The technical identity under the friendly name — the package name is what the removal
/// command actually acts on, so it is worth showing rather than hiding.
QString describe(const InstalledApp &app)
{
    // A package with no friendly name anywhere is already titled with its package name;
    // repeating it underneath would fill the row with the same string twice.
    QStringList parts;
    if (app.displayName != app.packageName)
        parts << app.packageName;
    if (!app.version.isEmpty())
        parts << QStringLiteral("v") + app.version;
    if (!app.removable)
        parts.prepend(Locale::tr(QStringLiteral("debloat.locked.note")));
    return parts.join(QStringLiteral(" · "));
}

} // namespace

DebloatPage::DebloatPage(QWidget *parent)
    : QWidget(parent)
    , m_scanner(new DebloatScanner(this))
    , m_engine(new ActionEngine(this))
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                                 Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    m_layout->setSpacing(Theme::Metric::SectionGap);

    auto *topBar = new QWidget(this);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(TopBarGap);

    m_summaryLabel = new QLabel(topBar);
    m_summaryLabel->setFont(Theme::Font::pageSub());
    QPalette pal = m_summaryLabel->palette();
    pal.setColor(QPalette::WindowText, Theme::Color::TextFaint());
    m_summaryLabel->setPalette(pal);
    topLayout->addWidget(m_summaryLabel);
    topLayout->addStretch(1);

    m_rescanButton = new PillButton(PillButton::Ghost,
                                    Locale::tr(QStringLiteral("debloat.rescan")), topBar);
    connect(m_rescanButton, &PillButton::clicked, this, [this] { rescan(); });
    topLayout->addWidget(m_rescanButton);

    m_selectAllButton = new PillButton(PillButton::Ghost,
                                       Locale::tr(QStringLiteral("debloat.selectAll")), topBar);
    connect(m_selectAllButton, &PillButton::clicked, this, &DebloatPage::toggleSelectAll);
    topLayout->addWidget(m_selectAllButton);

    m_bulkRemoveButton = new PillButton(
        PillButton::Accent, Locale::tr(QStringLiteral("debloat.removeSelected")).arg(0), topBar);
    connect(m_bulkRemoveButton, &PillButton::clicked, this, &DebloatPage::removeSelected);
    topLayout->addWidget(m_bulkRemoveButton);

    m_layout->addWidget(topBar);
    m_layout->addStretch(1);

    updateBulkBar();

    connect(m_scanner, &DebloatScanner::finished, this,
            [this](const QVector<InstalledApp> &apps) {
                m_scanning = false;
                m_lastApps = apps;
                rebuild(apps);
                Q_EMIT scanFinished();
            });

    connect(m_engine, &ActionEngine::started, this, [this](const QString &) {
        for (const QString &id : std::as_const(m_pendingIds))
            if (DebloatRow *row = m_rows.value(id))
                row->setBusy(true);
        m_bulkRemoveButton->setEnabledLook(false);
        m_selectAllButton->setEnabledLook(false);
        m_rescanButton->setEnabledLook(false);
    });

    connect(m_engine, &ActionEngine::finished, this,
            [this](const QString &, bool ok, const QString &output) {
                const QStringList spoken = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                QString summary =
                    spoken.isEmpty() ? (ok ? Locale::tr(QStringLiteral("actions.status.done"))
                                           : Locale::tr(QStringLiteral("actions.status.failed")))
                                     : spoken.last().trimmed();

                // The script reports its result as data (ARB-REMOVED|count|names) precisely
                // so the sentence can be built here, in whatever language is in use.
                if (summary.startsWith(QLatin1String("ARB-REMOVED|"))) {
                    const QStringList parts = summary.split(QLatin1Char('|'));
                    summary = Locale::tr(QStringLiteral("debloat.result"))
                                  .arg(parts.value(1))
                                  .arg(parts.value(2));
                    // The script now checks each package is actually gone from both the
                    // registered and the provisioned list before counting it. Anything
                    // Windows kept is named here rather than quietly folded into the
                    // "removed" total, which is what used to happen.
                    const QString kept = parts.value(3).trimmed();
                    if (!kept.isEmpty())
                        summary += QStringLiteral(" · ")
                                   + Locale::tr(QStringLiteral("debloat.resultKept")).arg(kept);
                }

                if (ok) {
                    m_pendingIds.clear();
                    Q_EMIT notice(summary);
                    rescan();   // the machine actually changed; re-derive the list from it
                } else {
                    for (const QString &id : std::as_const(m_pendingIds)) {
                        if (DebloatRow *row = m_rows.value(id)) {
                            row->setBusy(false);
                            row->setStatus(
                                Locale::tr(QStringLiteral("actions.detail")).arg(summary));
                        }
                    }
                    m_pendingIds.clear();
                    m_selectAllButton->setEnabledLook(true);
                    m_rescanButton->setEnabledLook(true);
                    updateBulkBar();
                    Q_EMIT notice(
                        Locale::tr(QStringLiteral("actions.notice.failed")).arg(summary));
                }
            });

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &DebloatPage::retranslate);

    rescan();
}

void DebloatPage::rescan()
{
    if (m_scanner->running())
        return;
    m_scanning = true;
    m_summaryLabel->setText(Locale::tr(QStringLiteral("debloat.scanning")));
    m_rescanButton->setEnabledLook(false);
    m_scanner->start();
}

void DebloatPage::rebuild(const QVector<InstalledApp> &apps)
{
    if (m_body) {
        m_layout->removeWidget(m_body);
        m_body->hide();
        m_body->deleteLater();
        m_body = nullptr;
    }
    m_rows.clear();
    m_sectionHeaders.clear();
    m_rowCount = int(apps.size());
    m_removableCount = 0;
    for (const InstalledApp &app : apps)
        if (app.removable)
            ++m_removableCount;

    m_body = new QWidget(this);
    auto *body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(Theme::Metric::SectionGap);

    if (apps.isEmpty()) {
        auto *empty = new QLabel(Locale::tr(QStringLiteral("debloat.empty")), m_body);
        empty->setWordWrap(true);
        empty->setFont(Theme::Font::pageSub());
        QPalette pal = empty->palette();
        pal.setColor(QPalette::WindowText, Theme::Color::TextFaint());
        empty->setPalette(pal);
        body->addWidget(empty);
    } else {
        for (int section = 0; section < SectionCount; ++section) {
            QVector<const InstalledApp *> here;
            for (const InstalledApp &app : apps)
                if (app.section() == section)
                    here.append(&app);
            if (here.isEmpty())
                continue;

            auto *header = new SectionHeader(
                Locale::tr(QString::fromLatin1(SectionKeys[section])), m_body);
            header->setCount(Locale::tr(QStringLiteral("debloat.count")).arg(here.size()));
            body->addWidget(header);
            m_sectionHeaders.append(header);

            auto *list = new QWidget(m_body);
            auto *listLayout = new QVBoxLayout(list);
            listLayout->setContentsMargins(0, 0, 0, 0);
            listLayout->setSpacing(1);

            for (const InstalledApp *app : std::as_const(here)) {
                auto *row = new DebloatRow(app->packageName, app->logo, app->displayName,
                                           describe(*app), list);
                row->setLocked(!app->removable);
                row->removeButton()->setText(Locale::tr(QStringLiteral("debloat.remove")));

                if (app->removable) {
                    const InstalledApp copy = *app;
                    connect(row->removeButton(), &PillButton::clicked, this,
                            [this, copy] { removeOne(copy); });
                    connect(row, &DebloatRow::toggled, this,
                            [this](const QString &, bool) { updateBulkBar(); });
                }
                listLayout->addWidget(row);
                m_rows.insert(app->packageName, row);
            }

            body->addWidget(list);
        }
    }

    m_layout->insertWidget(1, m_body);
    m_summaryLabel->setText(Locale::tr(QStringLiteral("debloat.summary"))
                                .arg(m_rowCount)
                                .arg(m_removableCount));
    // A rebuild can land in the middle of a removal — a language change is the way it
    // happens — and it throws away every row, taking their busy markers with it. Put the
    // in-flight state back rather than leaving the page looking idle while PowerShell is
    // still working.
    const bool busy = m_engine->running();
    if (busy) {
        for (const QString &id : std::as_const(m_pendingIds))
            if (DebloatRow *row = m_rows.value(id))
                row->setBusy(true);
    }
    m_rescanButton->setEnabledLook(!busy && !m_scanner->running());
    updateBulkBar();
}

void DebloatPage::updateBulkBar()
{
    int checked = 0;
    int selectable = 0;
    for (DebloatRow *row : std::as_const(m_rows)) {
        if (row->locked())
            continue;
        ++selectable;
        if (row->checked())
            ++checked;
    }

    m_bulkRemoveButton->setText(Locale::tr(QStringLiteral("debloat.removeSelected")).arg(checked));
    m_bulkRemoveButton->setEnabledLook(checked > 0 && !m_engine->running());
    m_selectAllButton->setEnabledLook(selectable > 0 && !m_engine->running());
    m_selectAllButton->setText(
        Locale::tr(checked > 0 && checked == selectable
                       ? QStringLiteral("debloat.clearSelection")
                       : QStringLiteral("debloat.selectAll")));
}

void DebloatPage::toggleSelectAll()
{
    int checked = 0;
    int selectable = 0;
    for (DebloatRow *row : std::as_const(m_rows)) {
        if (row->locked())
            continue;
        ++selectable;
        if (row->checked())
            ++checked;
    }

    const bool selectAll = checked < selectable;
    for (DebloatRow *row : std::as_const(m_rows))
        if (!row->locked())
            row->setChecked(selectAll);
    updateBulkBar();
}

void DebloatPage::removeOne(const InstalledApp &app)
{
    runRemoval({app.packageName}, {app.packageName},
               Locale::tr(QStringLiteral("debloat.confirm.single")).arg(app.displayName));
}

void DebloatPage::removeSelected()
{
    QStringList ids;
    for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it)
        if (!it.value()->locked() && it.value()->checked())
            ids << it.key();

    if (ids.isEmpty())
        return;
    runRemoval(ids, ids, Locale::tr(QStringLiteral("debloat.confirm.multi")).arg(ids.size()));
}

void DebloatPage::runRemoval(const QStringList &ids, const QStringList &packageNames,
                             const QString &confirmText)
{
    if (m_engine->running()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("actions.notice.busy")));
        return;
    }
    if (packageNames.isEmpty())
        return;

    Action action;
    action.id = QStringLiteral("debloat-run");
    action.name = Locale::tr(QStringLiteral("debloat.title"));
    action.desc = confirmText;
    action.reversible = false;
    action.note = Locale::tr(QStringLiteral("debloat.confirm.note"));
    action.run = DebloatActions::removalScript(packageNames).split(QLatin1Char('\n'));

    QMessageBox box(this);
    box.setWindowTitle(QCoreApplication::applicationName());
    box.setIcon(QMessageBox::NoIcon);
    box.setText(action.name);
    box.setInformativeText(action.desc + Locale::tr(QStringLiteral("actions.confirm.irreversible"))
                           + QStringLiteral("\n") + action.note);
    box.setDetailedText(action.script());

    QPushButton *go =
        box.addButton(Locale::tr(QStringLiteral("actions.run")), QMessageBox::AcceptRole);
    box.addButton(Locale::tr(QStringLiteral("actions.cancel")), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != go)
        return;

    m_pendingIds = ids;
    m_engine->run(action);
}

void DebloatPage::retranslate()
{
    m_rescanButton->setText(Locale::tr(QStringLiteral("debloat.rescan")));

    if (m_scanning) {
        m_summaryLabel->setText(Locale::tr(QStringLiteral("debloat.scanning")));
        m_selectAllButton->setText(Locale::tr(QStringLiteral("debloat.selectAll")));
        m_bulkRemoveButton->setText(Locale::tr(QStringLiteral("debloat.removeSelected")).arg(0));
    } else {
        rebuild(m_lastApps);
    }
}
