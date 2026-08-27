#include "mainwindow.h"

#include "appstate.h"
#include "catalog.h"
#include "deepinfo.h"
#include "i18n.h"
#include "theme.h"
#include "tweakengine.h"
#include "views/contentheader.h"
#include "views/overviewpage.h"
#include "views/sidebar.h"
#include "settings.h"
#include "updater.h"
#include "views/aboutpage.h"
#include "views/actionpage.h"
#include "views/debloatpage.h"
#include "views/journalpage.h"
#include "views/settingspage.h"
#include "views/statusbar.h"
#include "widgets/applyoverlay.h"
#include "views/titlebar.h"
#include "views/tilauncherpage.h"
#include "views/tweakpage.h"
#include "widgets/searchfield.h"
#include "widgets/smoothscrollarea.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QShortcut>
#include <QTimer>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

// Searches what is on screen as well as what is in the file: a user typing a translated
// word should find the row they can see, and one who knows the Turkish original (or is
// reading a support answer written against it) should still find it too.
bool matches(const Tweak &t, const QString &needle)
{
    return t.name.contains(needle, Qt::CaseInsensitive)
           || t.desc.contains(needle, Qt::CaseInsensitive)
           || t.displayName().contains(needle, Qt::CaseInsensitive)
           || t.displayDesc().contains(needle, Qt::CaseInsensitive);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowTitle(QCoreApplication::applicationName());

    m_engine = new TweakEngine(this);
    m_state = new AppState(m_engine, this);
    m_facts = SysInfo::collect();
    m_scannedAt = QDateTime::currentDateTime();
    m_monitor = new SystemMonitor(this);

    buildUi();
    wire();

    setCardMinimumSize({Theme::Metric::WindowWidth, Theme::Metric::WindowHeight});
    resize(minimumSize());

    m_titleBar->setSystemSummary(SysInfo::titleBarSummary(m_facts));

    const auto refreshStatusSummary = [this] {
        const int total = Catalog::instance().totalTweaks();
        m_statusBar->setSummary(total > 0
                                    ? Locale::tr(QStringLiteral("status.loaded")).arg(total)
                                    : Locale::tr(QStringLiteral("status.emptyCatalog")));
    };
    refreshStatusSummary();
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, refreshStatusSummary);
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &MainWindow::refreshView);
    // …and the counters, which is what fills the Genel Bakis card's five rows. Its title
    // was retranslated and its rows were not, so the card read half in one language and
    // half in the other until the next apply.
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this,
            &MainWindow::refreshCounters);
    // Both fact structs hold finished words, not raw readings — "Açık", "Şebeke",
    // "Dijital lisans", a date written the way the locale writes dates. Re-pushing the
    // struct we already had therefore retranslated the *labels* and left every *value*
    // in the language it was read in. The only way back is to read them again, which is
    // what collect() is cheap enough to do (it is what runs before the window is shown)
    // and what DeepInfo::Probe::retranslate() does without paying for its two PowerShell
    // stages a second time.
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        m_facts = SysInfo::collect();
        // collect() cannot fill activation, the restore point or the last update — those
        // come only from the async probe. Re-emitting its cached answer merges the three
        // back onto the freshly-collected struct (the resolved() handler below), so the
        // re-read does not blank them; without it they reverted to "—" on a language
        // change and stayed there.
        if (m_probe)
            m_probe->retranslate();
        m_titleBar->setSystemSummary(SysInfo::titleBarSummary(m_facts));
        m_overview->setFacts(m_facts);
        if (m_deepProbe)
            m_deepProbe->retranslate();
    });

    m_overview->setFacts(m_facts);
    m_monitor->start();

    refreshView();
    refreshCounters();

    m_probe = new SysInfo::Probe(this);
    connect(m_probe, &SysInfo::Probe::resolved, this,
            [this](const QString &activation, const QString &restorePoint, const QString &hotfix) {
                if (!activation.isEmpty())
                    m_facts.activation = activation;
                if (!restorePoint.isEmpty())
                    m_facts.lastRestorePoint = restorePoint;
                if (!hotfix.isEmpty())
                    m_facts.lastUpdate = hotfix;
                m_overview->setFacts(m_facts);
                m_settings->setRestorePoint(restorePoint);
            });
    m_probe->start();

    // The twelve blocks SysInfo cannot answer in one frame. Three stages, each repainting
    // the page as it lands, so the page is never waiting on the slowest of them.
    m_deepProbe = new DeepInfo::Probe(this);
    connect(m_deepProbe, &DeepInfo::Probe::updated, this,
            [this](DeepInfo::Probe::Stage) { m_overview->setDeepFacts(m_deepProbe->facts()); });
    m_deepProbe->start();

    // The Settings switch for this was stored and never consulted: Updater::check() had
    // exactly one caller, the manual button on the settings page. A little after the
    // window is up, so the first paint is not waiting on a network round trip.
    if (Settings::instance().checkUpdatesOnLaunch())
        QTimer::singleShot(2500, m_updater, [this] { m_updater->check(); });
}

void MainWindow::buildUi()
{
    auto *root = new QVBoxLayout(card());
    root->setContentsMargins(1, 1, 1, 1);   // the card's own 1px border
    root->setSpacing(0);

    m_titleBar = new TitleBar(card());
    root->addWidget(m_titleBar);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    root->addLayout(row, 1);

    m_sidebar = new Sidebar(m_state, card());
    row->addWidget(m_sidebar);

    auto *main = new QVBoxLayout;
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    row->addLayout(main, 1);

    m_header = new ContentHeader(card());
    main->addWidget(m_header);

    m_stack = new QStackedWidget(card());

    m_overviewScroll = new SmoothScrollArea(m_stack);
    m_overview = new OverviewPage(m_monitor, m_overviewScroll);
    m_overviewScroll->setWidget(m_overview);

    m_tweakScroll = new SmoothScrollArea(m_stack);
    m_tweaks = new TweakPage(m_state, m_tweakScroll);
    m_tweakScroll->setWidget(m_tweaks);

    m_updater = new Updater(this);
    m_settingsScroll = new SmoothScrollArea(m_stack);
    m_settings = new SettingsPage(m_state, m_updater, m_settingsScroll);
    m_settingsScroll->setWidget(m_settings);

    m_actionScroll = new SmoothScrollArea(m_stack);
    m_actions = new ActionPage(m_actionScroll);
    m_actionScroll->setWidget(m_actions);

    m_tiScroll = new SmoothScrollArea(m_stack);
    m_tiLauncher = new TiLauncherPage(m_tiScroll);
    m_tiScroll->setWidget(m_tiLauncher);

    m_debloatScroll = new SmoothScrollArea(m_stack);
    m_debloat = new DebloatPage(m_debloatScroll);
    m_debloatScroll->setWidget(m_debloat);

    m_journalScroll = new SmoothScrollArea(m_stack);
    m_journal = new JournalPage(m_engine, m_state, m_journalScroll);
    m_journalScroll->setWidget(m_journal);

    m_aboutScroll = new SmoothScrollArea(m_stack);
    m_about = new AboutPage(m_aboutScroll);
    m_aboutScroll->setWidget(m_about);

    m_stack->addWidget(m_overviewScroll);
    m_stack->addWidget(m_tweakScroll);
    m_stack->addWidget(m_settingsScroll);
    m_stack->addWidget(m_actionScroll);
    m_stack->addWidget(m_tiScroll);
    m_stack->addWidget(m_debloatScroll);
    m_stack->addWidget(m_journalScroll);
    m_stack->addWidget(m_aboutScroll);
    main->addWidget(m_stack, 1);

    m_statusBar = new StatusBar(card());
    main->addWidget(m_statusBar);

    // Sits above the content column, not in a layout: it covers the header, the list and
    // the status bar while a write is running, and leaves the title bar reachable.
    m_applyOverlay = new ApplyOverlay(m_state, card());
    m_applyOverlay->hide();
}

void MainWindow::wire()
{
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &FramelessWindow::toggleMaximize);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(this, &FramelessWindow::maximizedChanged, m_titleBar, &TitleBar::setMaximized);

    connect(m_sidebar, &Sidebar::categoryActivated, this, &MainWindow::onCategoryActivated);
    connect(m_sidebar->search(), &SearchField::textChanged, this, &MainWindow::onQueryChanged);

    connect(m_header, &ContentHeader::filterChanged, this, &MainWindow::onFilterChanged);
    connect(m_header, &ContentHeader::sortToggled, this, &MainWindow::onSortToggled);

    connect(m_statusBar, &StatusBar::applyRequested, this, &MainWindow::onApply);
    connect(m_applyOverlay, &ApplyOverlay::finished, this, &MainWindow::onApplyFinished);
    connect(m_applyOverlay, &ApplyOverlay::notice, m_statusBar, &StatusBar::setNotice);
    connect(m_statusBar, &StatusBar::revertRequested, this, &MainWindow::onRevert);

    connect(m_state, &AppState::pendingChanged, this, &MainWindow::refreshCounters);
    connect(m_settings, &SettingsPage::notice, m_statusBar, &StatusBar::setNotice);
    connect(m_actions, &ActionPage::notice, m_statusBar, &StatusBar::setNotice);
    connect(m_tiLauncher, &TiLauncherPage::notice, m_statusBar, &StatusBar::setNotice);
    connect(m_debloat, &DebloatPage::notice, m_statusBar, &StatusBar::setNotice);
    connect(m_journal, &JournalPage::notice, m_statusBar, &StatusBar::setNotice);
    // The scan runs in the background from construction on; if it lands while this page
    // happens to be the one on screen, the header's "N uygulama bulundu" needs a refresh.
    connect(m_debloat, &DebloatPage::scanFinished, this, [this] {
        // Every other sidebar row gets its count from the catalogue at build time; this
        // one only has a number to show once the machine has answered.
        const int found = m_debloat->rowCount();
        m_sidebar->setCategoryCount(Sidebar::debloatId(),
                                    found > 0 ? QString::number(found) : QString());
        if (m_state->selectedCategory() == Sidebar::debloatId())
            refreshView();
    });
    connect(m_settings, &SettingsPage::presetApplied, this,
            [this](const QString &name, int changed, int unknown) {
                QString text = Locale::tr(QStringLiteral("mw.preset.loaded"))
                                   .arg(name).arg(changed);
                if (unknown > 0)
                    text += Locale::tr(QStringLiteral("mw.preset.unknownSkipped")).arg(unknown);
                m_statusBar->setNotice(text);
                refreshView();
            });

    // Every widget paints its own colours and measures its own text, so a palette swap
    // or a change of interface face means a full repaint of the tree.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        const QList<QWidget *> all = findChildren<QWidget *>();
        for (QWidget *w : all) {
            w->updateGeometry();
            w->update();
        }
        update();
    });
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, [this] {
        const QList<QWidget *> all = findChildren<QWidget *>();
        for (QWidget *w : all)
            w->update();
        update();
    });

    auto *focusSearch = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this);
    connect(focusSearch, &QShortcut::activated, this, [this] { m_sidebar->search()->focusField(); });

    auto *clearSearch = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(clearSearch, &QShortcut::activated, this, [this] { m_sidebar->search()->clearText(); });
    // A window shortcut consumes the key before any widget sees a key press, so this one
    // was eating the Escape the apply overlay listens for — the overlay could not be
    // dismissed with the keyboard, and Escape during a run cleared the search box behind
    // the scrim instead. Disabling the shortcut object is the only thing that works;
    // returning early from the handler is too late, the key is already gone.
    // Negated: the signal carries "the overlay is visible", and the shortcut has to be
    // OFF exactly then. Wiring it straight to setEnabled did the opposite — it turned the
    // shortcut on while the overlay was up (so Escape still cleared the search behind the
    // scrim) and off the moment it closed (so Escape stopped clearing the search at all
    // for the rest of the session).
    connect(m_applyOverlay, &ApplyOverlay::visibilityChanged, clearSearch,
            [clearSearch](bool visible) { clearSearch->setEnabled(!visible); });
    clearSearch->setEnabled(!m_applyOverlay->isVisible());
}

QVector<Section> MainWindow::visibleSections() const
{
    const Catalog &catalog = Catalog::instance();
    const Filter filter = m_state->filter();

    const auto keep = [this, filter](const Tweak &t) {
        switch (filter) {
        case Filter::Enabled: return m_state->isOn(t.id);
        case Filter::Changed: return m_state->isPending(t.id);
        case Filter::All:     break;
        }
        return true;
    };

    QVector<Section> result;

    if (m_state->searching()) {
        // Search spans the whole catalogue; each category becomes one section.
        const QString needle = m_state->query().trimmed();
        for (const Category &c : catalog.categories()) {
            Section hits;
            hits.title = Locale::tr(QStringLiteral("category.") + c.id);
            for (const Section &s : c.sections)
                for (const Tweak &t : s.tweaks)
                    if (keep(t) && matches(t, needle))
                        hits.tweaks.append(t);
            if (!hits.tweaks.isEmpty())
                result.append(hits);
        }
    } else {
        const Category *c = catalog.category(m_state->selectedCategory());
        if (!c)
            return result;
        for (const Section &s : c->sections) {
            // Copied whole and emptied, rather than field by field. Copying `title` alone
            // silently dropped `titleKey`, and the two synthesised categories — Services
            // and Startup — carry their heading only as a key, so both rendered under a
            // blank header in every language. Anything added to Section from here on comes
            // along on its own.
            Section kept = s;
            kept.tweaks.clear();
            for (const Tweak &t : s.tweaks)
                if (keep(t))
                    kept.tweaks.append(t);
            if (!kept.tweaks.isEmpty())
                result.append(kept);
        }
    }

    if (m_alphabetical) {
        for (Section &s : result)
            // By what the row actually says, not by the Turkish in catalog.json: sorting
            // an English interface by its Turkish names puts the list in an order with no
            // relation to anything on screen.
            std::sort(s.tweaks.begin(), s.tweaks.end(), [](const Tweak &a, const Tweak &b) {
                return a.displayName().localeAwareCompare(b.displayName()) < 0;
            });
    }

    return result;
}

void MainWindow::refreshView()
{
    const Catalog &catalog = Catalog::instance();
    const Category *category = catalog.category(m_state->selectedCategory());
    const bool searching = m_state->searching();
    const QString current = m_state->selectedCategory();
    const bool settings = !searching && current == Sidebar::settingsId();
    const bool actions = !searching && current == Sidebar::actionsId();
    const bool debloat = !searching && current == Sidebar::debloatId();
    const bool journal = !searching && current == Sidebar::journalId();
    const bool tiLauncher = !searching && current == Sidebar::tiLauncherId();
    const bool about = !searching && current == Sidebar::aboutId();
    const bool overview = !searching && category && category->isOverview();

    m_header->setControlsVisible(!overview && !settings && !about && !debloat && !tiLauncher);

    if (about) {
        m_header->setTitle(Locale::tr(QStringLiteral("sidebar.about")));
        m_header->setSubtitle(Locale::tr(QStringLiteral("about.subtitle")));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_aboutScroll);
        return;
    }

    if (journal) {
        // Re-read on every visit: an apply since the last one will have added to it.
        m_journal->reload();
        m_header->setTitle(Locale::tr(QStringLiteral("journal.title")));
        m_header->setSubtitle(m_journal->rowCount() > 0
                                  ? Locale::tr(QStringLiteral("journal.subtitle"))
                                        .arg(m_journal->rowCount())
                                  : Locale::tr(QStringLiteral("journal.empty.status")));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_journalScroll);
        return;
    }

    if (actions) {
        m_header->setTitle(Locale::tr(QStringLiteral("actions.title")));
        m_header->setSubtitle(Locale::tr(QStringLiteral("actions.subtitle"))
                                  .arg(m_actions->rowCount()));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_actionScroll);
        return;
    }

    if (tiLauncher) {
        m_header->setTitle(Locale::tr(QStringLiteral("ti.title")));
        m_header->setSubtitle(Locale::tr(QStringLiteral("ti.subtitle")));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_tiScroll);
        return;
    }

    if (debloat) {
        m_header->setTitle(Locale::tr(QStringLiteral("sidebar.debloat")));
        m_header->setSubtitle(m_debloat->scanning()
                                  ? Locale::tr(QStringLiteral("debloat.scanning"))
                                  : Locale::tr(QStringLiteral("debloat.subtitle"))
                                        .arg(m_debloat->rowCount())
                                        .arg(m_debloat->removableCount()));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_debloatScroll);
        return;
    }

    if (settings) {
        m_header->setTitle(Locale::tr(QStringLiteral("sidebar.settings")));
        m_header->setSubtitle(Locale::tr(QStringLiteral("mw.page.settings.subtitle"))
                                  .arg(m_settings->rowCount())
                                  .arg(QCoreApplication::applicationVersion()));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_settingsScroll);
        return;
    }

    if (overview) {
        m_header->setTitle(Locale::tr(QStringLiteral("category.") + category->id));
        m_header->setSubtitle(Locale::tr(QStringLiteral("mw.page.overview.subtitle"))
                                  .arg(m_facts.computerName,
                                       SysInfo::friendlyDateTime(m_scannedAt).toLower()));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_overviewScroll);
        return;
    }

    const QVector<Section> sections = visibleSections();
    QString emptyMessage;

    if (searching) {
        int hits = 0;
        for (const Section &s : sections)
            hits += s.tweaks.size();
        m_header->setTitle(Locale::tr(QStringLiteral("content.search.title")));
        // One multi-argument arg(), not two chained ones: chaining substitutes the query
        // first and then rescans it, so typing "%2" into the search box put the hit count
        // inside the user's own text and left the real marker unreplaced.
        m_header->setSubtitle(Locale::tr(QStringLiteral("content.search.subtitle"))
                                  .arg(m_state->query().trimmed(), QString::number(hits)));
        m_header->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount()));
    } else if (!category) {
        // Every pinned page returned above and the overview did too, so getting here with
        // no category means an id nothing knows — the --category switch is the way in.
        // Dereferencing it was an outright crash.
        m_header->setTitle(Locale::tr(QStringLiteral("content.search.title")));
        m_header->setSubtitle(Locale::tr(QStringLiteral("content.emptyCategory")));
        m_header->setPendingLabel({});
        m_header->setControlsVisible(false);
        emptyMessage = Locale::tr(QStringLiteral("content.emptyPage"));
    } else if (category->tweakCount() == 0) {
        // The catalogue is being rebuilt one page at a time; an empty category says so
        // rather than offering filters over nothing.
        m_header->setTitle(Locale::tr(QStringLiteral("category.") + category->id));
        m_header->setSubtitle(Locale::tr(QStringLiteral("content.emptyCategory")));
        m_header->setPendingLabel({});
        m_header->setControlsVisible(false);
        emptyMessage = Locale::tr(QStringLiteral("content.emptyPage"));
    } else {
        // Tweaks this build ignores are counted out loud rather than quietly listed as
        // if they worked — the rows say so too, but the header is where you look first.
        int unsupported = 0;
        for (const Section &section : category->sections)
            for (const Tweak &tweak : section.tweaks)
                if (!tweak.applicable)
                    ++unsupported;

        QString subtitle = Locale::tr(QStringLiteral("content.categorySubtitle"))
                               .arg(category->tweakCount())
                               .arg(m_state->appliedCount(*category));
        if (unsupported > 0)
            subtitle = Locale::tr(QStringLiteral("content.categorySubtitleIncompatible"))
                           .arg(category->tweakCount())
                           .arg(m_state->appliedCount(*category))
                           .arg(unsupported);

        m_header->setTitle(Locale::tr(QStringLiteral("category.") + category->id));
        m_header->setSubtitle(subtitle);
        m_header->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount(*category)));
    }

    m_tweaks->setSections(sections, emptyMessage);
    m_stack->setCurrentWidget(m_tweakScroll);
}

void MainWindow::refreshCounters()
{
    m_statusBar->setPending(m_state->pendingCount());
    refreshOverviewCatalog();

    const Category *category = Catalog::instance().category(m_state->selectedCategory());
    if (Sidebar::isPinnedPage(m_state->selectedCategory()))
        m_header->setPendingLabel({});
    else if (m_state->searching())
        m_header->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount()));
    else if (category && !category->isOverview())
        m_header->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount(*category)));
}

void MainWindow::refreshOverviewCatalog()
{
    const Catalog &catalog = Catalog::instance();

    int applied = 0;
    int best = 0;
    QString busiest;
    for (const Category &category : catalog.categories()) {
        int here = 0;
        for (const Section &section : category.sections)
            for (const Tweak &tweak : section.tweaks)
                if (m_state->isOn(tweak.id))
                    ++here;
        applied += here;
        if (here > best) {
            best = here;
            // The category name in the interface language: every other place that shows
            // one goes through this lookup, and this tile was the one reading the raw
            // Turkish out of catalog.json.
            busiest = QStringLiteral("%1 · %2")
                          .arg(Locale::tr(QStringLiteral("category.") + category.id))
                          .arg(here);
        }
    }

    m_overview->setCatalogState(catalog.totalTweaks(), applied, m_state->pendingCount(), busiest);
}

void MainWindow::showCategory(const QString &id)
{
    if (Catalog::instance().category(id) || Sidebar::isPinnedPage(id))
        onCategoryActivated(id);
}

void MainWindow::showSearch(const QString &text)
{
    m_sidebar->search()->setText(text);
}

void MainWindow::onCategoryActivated(const QString &id)
{
    // Picking a category is an explicit "show me this", so it leaves the search behind.
    if (m_state->searching())
        m_sidebar->search()->clearText();

    m_state->setSelectedCategory(id);
    m_sidebar->setSelected(id);
    m_tweakScroll->scrollToTop();
    m_overviewScroll->scrollToTop();
    refreshView();
}

void MainWindow::onQueryChanged(const QString &query)
{
    m_state->setQuery(query);
    m_tweakScroll->scrollToTop();
    refreshView();
}

void MainWindow::onFilterChanged(int index)
{
    m_state->setFilter(static_cast<Filter>(index));
    m_tweakScroll->scrollToTop();
    refreshView();
}

void MainWindow::onSortToggled(bool alphabetical)
{
    m_alphabetical = alphabetical;
    refreshView();
}

void MainWindow::syncOverlayGeometry()
{
    // Null for the whole first half of the constructor, and a resize can arrive there.
    if (m_applyOverlay)
        m_applyOverlay->setGeometry(overlayRect());
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    FramelessWindow::resizeEvent(e);
    syncOverlayGeometry();
}

void MainWindow::changeEvent(QEvent *e)
{
    FramelessWindow::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange)
        syncOverlayGeometry();
}

QRect MainWindow::overlayRect() const
{
    // Everything below the title bar, sidebar included: while a write is running the
    // user should not be able to navigate away from it. The title bar stays reachable
    // so the window can still be moved or minimised.
    const QRect inner = card()->rect().adjusted(1, 1, -1, -1);
    return inner.adjusted(0, m_titleBar->height(), 0, 0);
}

void MainWindow::onApply()
{
    if (m_state->pendingCount() == 0 || m_applyOverlay->running())
        return;

    // Settings offers this switch and nothing used to read it, so the confirmation it
    // promises never appeared — Uygula went straight to the writes whichever way it was
    // set. It defaults to on, which is why this is the path most users were missing.
    if (Settings::instance().confirmBeforeApply()) {
        QMessageBox box(this);
        box.setWindowTitle(QCoreApplication::applicationName());
        box.setIcon(QMessageBox::NoIcon);
        box.setText(Locale::tr(QStringLiteral("mw.confirm.title")).arg(m_state->pendingCount()));
        box.setInformativeText(Locale::tr(QStringLiteral("mw.confirm.body")));
        QAbstractButton *go = box.addButton(Locale::tr(QStringLiteral("mw.confirm.apply")),
                                            QMessageBox::AcceptRole);
        box.addButton(Locale::tr(QStringLiteral("mw.confirm.cancel")), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() != go)
            return;
    }

    // The overlay drives the writes itself, one per tick, and reports back when done.
    m_applyOverlay->setGeometry(overlayRect());
    m_applyOverlay->run();
}

void MainWindow::onApplyFinished(int succeeded, int failed, bool elevationRequired,
                                 const QString &firstError)
{
    refreshCounters();
    refreshView();

    if (failed == 0) {
        if (succeeded > 0)
            m_statusBar->setNotice(Locale::tr(QStringLiteral("mw.notice.applied")).arg(succeeded));
        return;
    }

    const AppState::ApplyReport report{succeeded, failed, elevationRequired, firstError};

    if (report.elevationRequired && !TweakEngine::isElevated()) {
        // These tweaks live outside HKCU, so they cannot be written by a standard token.
        QMessageBox box(this);
        box.setWindowTitle(QCoreApplication::applicationName());
        box.setIcon(QMessageBox::NoIcon);
        box.setText(Locale::tr(QStringLiteral("mw.elevate.title")).arg(report.failed));
        box.setInformativeText(Locale::tr(QStringLiteral("mw.elevate.body")));
        QAbstractButton *restart = box.addButton(Locale::tr(QStringLiteral("mw.elevate.restart")),
                                                 QMessageBox::AcceptRole);
        box.addButton(Locale::tr(QStringLiteral("mw.elevate.later")), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == restart) {
            m_state->stashPending();
            if (TweakEngine::relaunchElevated())
                close();
            else
                m_statusBar->setNotice(Locale::tr(QStringLiteral("mw.notice.restartCancelled")));
        } else {
            m_statusBar->setNotice(Locale::tr(QStringLiteral("mw.notice.elevatePending")).arg(report.failed));
        }
        return;
    }

    m_statusBar->setNotice(report.firstError.isEmpty()
                               ? Locale::tr(QStringLiteral("mw.notice.applyFailed")).arg(report.failed)
                               : Locale::tr(QStringLiteral("mw.notice.applyFailedDetail"))
                                     .arg(report.failed).arg(report.firstError));
}

void MainWindow::onRevert()
{
    m_state->revertPending();
    refreshView();
}

