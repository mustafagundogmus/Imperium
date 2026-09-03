#include "mainwindow.h"

#include "appstate.h"
#include "catalog.h"
#include "deepinfo.h"
#include "i18n.h"
#include "theme.h"
#include "tweakengine.h"
#include "fluent/fluentchrome.h"
#include "views/chrome.h"
#include "views/classicchrome.h"
#include "views/overviewpage.h"
#include "views/sidebar.h"
#include "settings.h"
#include "updater.h"
#include "winpaths.h"
#include "views/aboutpage.h"
#include "views/actionpage.h"
#include "views/cleanerpage.h"
#include "views/debloatpage.h"
#include "views/godmodepage.h"
#include "views/journalpage.h"
#include "views/settingspage.h"
#include "widgets/applyoverlay.h"
#include "widgets/dialog.h"
#include "widgets/splashscreen.h"
#include "widgets/updatedialog.h"
#include "views/tilauncherpage.h"
#include "views/tweakpage.h"
#include "widgets/smoothscrollarea.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QLayout>
#include <QProcess>
#include <QShortcut>
#include <QTimer>
#include <QStackedWidget>
#include <QUrl>

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

    // Each of these is a stage the splash names while it runs, and each call repaints it.
    // Before 0.12.0 the whole constructor ran with the event loop untouched, so the card
    // froze on its first frame — and on a machine where one of these stages takes tens of
    // seconds, a frozen splash is indistinguishable from a hung application. Naming them
    // also means a stall can be reported by whoever hits it: the card says which one.
    Splash::report(QStringLiteral("splash.stage.catalog"));
    m_engine = new TweakEngine(this);
    m_state = new AppState(m_engine, this);

    Splash::report(QStringLiteral("splash.stage.facts"));
    m_facts = SysInfo::collect();
    m_scannedAt = QDateTime::currentDateTime();
    m_monitor = new SystemMonitor(this);

    Splash::report(QStringLiteral("splash.stage.ui"));
    buildUi();
    wire();

    applyShellMetrics();

    m_chrome->setSystemSummary(SysInfo::titleBarSummary(m_facts));

    const auto refreshStatusSummary = [this] {
        const int total = Catalog::instance().totalTweaks();
        m_chrome->setSummary(total > 0
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
        m_chrome->setSystemSummary(SysInfo::titleBarSummary(m_facts));
        m_overview->setFacts(m_facts);
        if (m_deepProbe)
            m_deepProbe->retranslate();
    });

    m_overview->setFacts(m_facts);
    m_monitor->start();

    Splash::report(QStringLiteral("splash.stage.state"));
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
                m_chrome->setRestorePoint(m_facts.lastRestorePoint);
            });
    m_probe->start();

    // The twelve blocks SysInfo cannot answer in one frame. Three stages, each repainting
    // the page as it lands, so the page is never waiting on the slowest of them.
    m_deepProbe = new DeepInfo::Probe(this);
    connect(m_deepProbe, &DeepInfo::Probe::updated, this,
            [this](DeepInfo::Probe::Stage) { m_overview->setDeepFacts(m_deepProbe->facts()); });
    m_deepProbe->start();

    // Whatever a previous self-update left in the folder. It normally fails on the first
    // try and that is expected rather than a fault: the process being replaced is still
    // exiting, and Windows will not delete the file it is running from. The second attempt
    // a few seconds later is the one that lands. Both are two QFile::exists() calls when
    // there is nothing to do, so the ordinary launch pays nothing for them.
    if (!Updater::sweepPreviousInstall())
        QTimer::singleShot(6000, this, [] { Updater::sweepPreviousInstall(); });

    // The Settings switch for this was stored and never consulted: Updater::check() had
    // exactly one caller, the manual button on the settings page. A little after the
    // window is up, so the first paint is not waiting on a network round trip.
    if (Settings::instance().checkUpdatesOnLaunch() && Updater::launchCheckDue())
        QTimer::singleShot(2500, m_updater, [this] { m_updater->check(); });
}

void MainWindow::buildUi()
{
    // The pages, once. The chrome around them is built last and can be built again.
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

    m_cleanerScroll = new SmoothScrollArea(m_stack);
    m_cleaner = new CleanerPage(m_cleanerScroll);
    m_cleanerScroll->setWidget(m_cleaner);

    m_godModeScroll = new SmoothScrollArea(m_stack);
    m_godMode = new GodModePage(m_godModeScroll);
    m_godModeScroll->setWidget(m_godMode);

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
    m_stack->addWidget(m_cleanerScroll);
    m_stack->addWidget(m_godModeScroll);
    m_stack->addWidget(m_journalScroll);
    m_stack->addWidget(m_aboutScroll);

    // Sits above the content column, not in a layout: it covers the header, the list and
    // the status bar while a write is running, and leaves the title bar reachable.
    m_applyOverlay = new ApplyOverlay(m_state, card());
    m_applyOverlay->hide();

    buildChrome();
}

void MainWindow::buildChrome()
{
    // The stack is the one thing that survives: it is taken back onto the card before
    // the old chrome — which may have adopted it into a column of its own — is deleted
    // with everything it built.
    if (m_chrome) {
        m_stack->setParent(card());
        delete m_chrome;
        m_chrome = nullptr;
    }
    delete card()->layout();

    if (Theme::fluent())
        m_chrome = new FluentChrome(m_state, this);
    else
        m_chrome = new ClassicChrome(m_state, this);
    m_chrome->build(card(), m_stack);
    wireChrome();

    // The overlay is a hand-placed child of the card; the chrome's widgets were created
    // after it and would otherwise sit on top.
    m_applyOverlay->raise();
    syncOverlayGeometry();

    m_chrome->setMaximized(isMaximized());
    m_chrome->setSelected(m_state->selectedCategory());
    m_chrome->setPending(m_state->pendingCount());
    m_chrome->setRestorePoint(m_facts.lastRestorePoint);
    m_chrome->setSystemSummary(SysInfo::titleBarSummary(m_facts));
    if (m_monitor)
        m_chrome->setSample(m_monitor->latest());
    if (m_cleaner && m_cleaner->reclaimableBytes() > 0)
        m_chrome->setCategoryCount(Sidebar::cleanerId(), m_cleaner->reclaimableText());
    if (m_debloat && m_debloat->rowCount() > 0)
        m_chrome->setCategoryCount(Sidebar::debloatId(), QString::number(m_debloat->rowCount()));
    const int total = Catalog::instance().totalTweaks();
    m_chrome->setSummary(total > 0 ? Locale::tr(QStringLiteral("status.loaded")).arg(total)
                                   : Locale::tr(QStringLiteral("status.emptyCatalog")));
}

void MainWindow::wireChrome()
{
    connect(m_chrome, &Chrome::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_chrome, &Chrome::maximizeToggleRequested, this, &FramelessWindow::toggleMaximize);
    connect(m_chrome, &Chrome::closeRequested, this, &QWidget::close);
    connect(this, &FramelessWindow::maximizedChanged, m_chrome, &Chrome::setMaximized);

    connect(m_chrome, &Chrome::categoryActivated, this, &MainWindow::onCategoryActivated);
    connect(m_chrome, &Chrome::queryChanged, this, &MainWindow::onQueryChanged);
    connect(m_chrome, &Chrome::filterChanged, this, &MainWindow::onFilterChanged);
    connect(m_chrome, &Chrome::sortToggled, this, &MainWindow::onSortToggled);
    connect(m_chrome, &Chrome::applyRequested, this, &MainWindow::onApply);
    connect(m_chrome, &Chrome::revertRequested, this, &MainWindow::onRevert);

    // The Fluent pane's "Oluştur": the same door the settings page opens — Windows' own
    // System Protection dialog, since this app deliberately creates no restore point itself.
    connect(m_chrome, &Chrome::restorePointRequested, this, [] {
        const QString protection =
            WinPaths::system32() + QStringLiteral("\\SystemPropertiesProtection.exe");
        if (!QFileInfo::exists(protection) || !QProcess::startDetached(protection, {}))
            QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:about")));
    });
}

void MainWindow::applyShellMetrics()
{
    using namespace Theme;
    const bool fluentShell = Theme::fluent();
    const QSize minimum = fluentShell ? QSize(Fluent::MinWidth, Fluent::MinHeight)
                                      : QSize(Metric::WindowWidth, Metric::WindowHeight);
    const QSize opening = fluentShell ? QSize(Fluent::WindowWidth, Fluent::WindowHeight) : minimum;
    setCardMinimumSize(minimum);

    // minimumSize() is the card's minimum plus the shadow margins; the difference is the
    // margin, which the opening size needs added the same way.
    const QSize margins = minimumSize() - minimum;
    if (!isMaximized()) {
        const QSize target = opening + margins;
        if (width() < target.width() || height() < target.height())
            resize(qMax(width(), target.width()), qMax(height(), target.height()));
    }
}

void MainWindow::showNotice(const QString &text)
{
    if (m_chrome)
        m_chrome->setNotice(text);
}

void MainWindow::wire()
{
    connect(m_applyOverlay, &ApplyOverlay::finished, this, &MainWindow::onApplyFinished);
    connect(m_applyOverlay, &ApplyOverlay::notice, this, &MainWindow::showNotice);

    connect(m_state, &AppState::pendingChanged, this, &MainWindow::refreshCounters);
    connect(m_settings, &SettingsPage::notice, this, &MainWindow::showNotice);
    connect(m_actions, &ActionPage::notice, this, &MainWindow::showNotice);
    connect(m_tiLauncher, &TiLauncherPage::notice, this, &MainWindow::showNotice);
    connect(m_debloat, &DebloatPage::notice, this, &MainWindow::showNotice);
    connect(m_cleaner, &CleanerPage::notice, this, &MainWindow::showNotice);
    // The cleaner's sidebar count is a size, not a number of rows: what a clean would
    // free right now, refreshed after every scan — and every clean ends in a scan.
    connect(m_cleaner, &CleanerPage::scanFinished, this, [this] {
        const qint64 bytes = m_cleaner->reclaimableBytes();
        m_chrome->setCategoryCount(Sidebar::cleanerId(),
                                    bytes > 0 ? m_cleaner->reclaimableText() : QString());
        if (m_state->selectedCategory() == Sidebar::cleanerId())
            refreshView();
    });
    connect(m_godMode, &GodModePage::notice, this, &MainWindow::showNotice);
    connect(m_journal, &JournalPage::notice, this, &MainWindow::showNotice);
    // The scan runs in the background from construction on; if it lands while this page
    // happens to be the one on screen, the header's "N uygulama bulundu" needs a refresh.
    connect(m_debloat, &DebloatPage::scanFinished, this, [this] {
        // Every other sidebar row gets its count from the catalogue at build time; this
        // one only has a number to show once the machine has answered.
        const int found = m_debloat->rowCount();
        m_chrome->setCategoryCount(Sidebar::debloatId(),
                                    found > 0 ? QString::number(found) : QString());
        if (m_state->selectedCategory() == Sidebar::debloatId())
            refreshView();
    });
    // The offer, for both kinds of check.
    //
    // It used to be the settings page's business, because the only thing finding a new
    // version did was rewrite one row and — for a check somebody had pressed a button
    // for — open a browser. Now that the answer is "shall I replace myself", it belongs
    // to the window: the dialog is application-modal, it outlives the page it might have
    // been opened from, and the launch-time check has no page on screen at all. The
    // settings row still updates itself; this is the part that asks.
    //
    // Both kinds of check ask. The old rule that only a user-initiated check was allowed
    // to take over the screen existed because taking it over meant launching a browser
    // seconds after startup; a dialog naming the version and waiting for an answer is the
    // thing the launch check was always for, and one nobody is ever told about is the
    // whole failure this feature exists to fix.
    connect(m_updater, &Updater::finished, this,
            [this](bool available, const QString &, const QString &, const QString &error, bool) {
                // …and not while one is already being installed: offer() returns as soon
                // as the question is answered, so m_offering alone would go false again
                // while the download it started is still running behind its own dialog.
                if (!available || !error.isEmpty() || m_offering || m_updater->installing())
                    return;
                m_offering = true;
                // Off the network reply's stack before a modal dialog spins an event loop
                // on top of it, and with the release copied out rather than read back
                // later, so a second check cannot change the answer under the dialog.
                const Updater::Release release = m_updater->latest();
                QTimer::singleShot(0, this, [this, release] {
                    UpdateDialog::offer(m_updater, release, this);
                    m_offering = false;
                });
            });

    connect(m_settings, &SettingsPage::presetApplied, this,
            [this](const QString &name, int changed, int unknown) {
                QString text = Locale::tr(QStringLiteral("mw.preset.loaded"))
                                   .arg(name).arg(changed);
                if (unknown > 0)
                    text += Locale::tr(QStringLiteral("mw.preset.unknownSkipped")).arg(unknown);
                showNotice(text);
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
    connect(focusSearch, &QShortcut::activated, this, [this] { m_chrome->focusSearch(); });

    auto *clearSearch = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(clearSearch, &QShortcut::activated, this, [this] { m_chrome->clearSearch(); });
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
    // Same for Ctrl+K: the search field sits behind the scrim, and typing into it
    // rebuilds the list while the writes are still being drained.
    connect(m_applyOverlay, &ApplyOverlay::visibilityChanged, focusSearch,
            [focusSearch](bool visible) { focusSearch->setEnabled(!visible); });
    focusSearch->setEnabled(!m_applyOverlay->isVisible());

    // The Fluent pane's status card reads the same sampler the overview does.
    connect(m_monitor, &SystemMonitor::sampled, this, [this](const Sample &sample) {
        if (m_chrome)
            m_chrome->setSample(sample);
    });

    // A shell switch rebuilds the chrome around the same pages, then puts the page that
    // was on screen back into the new header and list.
    connect(Theme::notifier(), &Theme::Notifier::shellChanged, this, [this] {
        buildChrome();
        applyShellMetrics();
        refreshView();
        refreshCounters();
    });
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
            hits.categoryId = c.id;
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
            kept.categoryId = c->id;
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
    const bool cleaner = !searching && current == Sidebar::cleanerId();
    const bool godMode = !searching && current == Sidebar::godModeId();
    const bool journal = !searching && current == Sidebar::journalId();
    const bool tiLauncher = !searching && current == Sidebar::tiLauncherId();
    const bool about = !searching && current == Sidebar::aboutId();
    const bool overview = !searching && category && category->isOverview();

    // The filter and the sort act on the tweak list and nothing else; the journal and the
    // actions had been showing them over lists they could not filter.
    m_chrome->setControlsVisible(!overview && !settings && !about && !debloat && !cleaner
                                 && !tiLauncher && !godMode && !journal && !actions);

    if (about) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("sidebar.about")));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("about.subtitle")));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_aboutScroll);
        return;
    }

    if (journal) {
        // Re-read on every visit: an apply since the last one will have added to it.
        m_journal->reload();
        m_chrome->setTitle(Locale::tr(QStringLiteral("journal.title")));
        m_chrome->setSubtitle(m_journal->rowCount() > 0
                                  ? Locale::tr(QStringLiteral("journal.subtitle"))
                                        .arg(m_journal->rowCount())
                                  : Locale::tr(QStringLiteral("journal.empty.status")));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_journalScroll);
        return;
    }

    if (actions) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("actions.title")));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("actions.subtitle"))
                                  .arg(m_actions->rowCount()));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_actionScroll);
        return;
    }

    if (tiLauncher) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("ti.title")));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("ti.subtitle")));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_tiScroll);
        return;
    }

    if (godMode) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("godmode.title")));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("godmode.subtitle"))
                                  .arg(m_godMode->rowCount()));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_godModeScroll);
        return;
    }

    if (debloat) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("sidebar.debloat")));
        m_chrome->setSubtitle(m_debloat->scanning()
                                  ? Locale::tr(QStringLiteral("debloat.scanning"))
                                  : Locale::tr(QStringLiteral("debloat.subtitle"))
                                        .arg(m_debloat->rowCount())
                                        .arg(m_debloat->removableCount()));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_debloatScroll);
        return;
    }

    if (cleaner) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("sidebar.cleaner")));
        m_chrome->setSubtitle(m_cleaner->scanning()
                                  ? Locale::tr(QStringLiteral("cleaner.scanning"))
                                  : Locale::tr(QStringLiteral("cleaner.subtitle"))
                                        .arg(m_cleaner->rowCount())
                                        .arg(m_cleaner->reclaimableText()));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_cleanerScroll);
        return;
    }

    if (settings) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("sidebar.settings")));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("mw.page.settings.subtitle"))
                                  .arg(m_settings->rowCount())
                                  .arg(QCoreApplication::applicationVersion()));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_settingsScroll);
        return;
    }

    if (overview) {
        m_chrome->setTitle(Locale::tr(QStringLiteral("category.") + category->id));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("mw.page.overview.subtitle"))
                                  .arg(m_facts.computerName,
                                       SysInfo::friendlyDateTime(m_scannedAt).toLower()));
        m_chrome->setPendingLabel({});
        m_stack->setCurrentWidget(m_overviewScroll);
        return;
    }

    const QVector<Section> sections = visibleSections();
    QString emptyMessage;

    if (searching) {
        int hits = 0;
        for (const Section &s : sections)
            hits += s.tweaks.size();
        m_chrome->setTitle(Locale::tr(QStringLiteral("content.search.title")));
        // One multi-argument arg(), not two chained ones: chaining substitutes the query
        // first and then rescans it, so typing "%2" into the search box put the hit count
        // inside the user's own text and left the real marker unreplaced.
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("content.search.subtitle"))
                                  .arg(m_state->query().trimmed(), QString::number(hits)));
        m_chrome->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount()));
        // The filter's counts are over the whole catalogue while a search is on, since
        // that is what the search spans.
        int on = 0;
        forEachTweak(catalog, [&](const Tweak &t) { on += m_state->isOn(t.id) ? 1 : 0; });
        m_chrome->setFilterCounts(catalog.totalTweaks(), on, m_state->pendingCount());
    } else if (!category) {
        // Every pinned page returned above and the overview did too, so getting here with
        // no category means an id nothing knows — the --category switch is the way in.
        // Dereferencing it was an outright crash.
        m_chrome->setTitle(Locale::tr(QStringLiteral("content.search.title")));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("content.emptyCategory")));
        m_chrome->setPendingLabel({});
        m_chrome->setControlsVisible(false);
        emptyMessage = Locale::tr(QStringLiteral("content.emptyPage"));
    } else if (category->tweakCount() == 0) {
        // The catalogue is being rebuilt one page at a time; an empty category says so
        // rather than offering filters over nothing.
        m_chrome->setTitle(Locale::tr(QStringLiteral("category.") + category->id));
        m_chrome->setSubtitle(Locale::tr(QStringLiteral("content.emptyCategory")));
        m_chrome->setPendingLabel({});
        m_chrome->setControlsVisible(false);
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

        m_chrome->setTitle(Locale::tr(QStringLiteral("category.") + category->id));
        m_chrome->setSubtitle(subtitle);
        m_chrome->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount(*category)));
        m_chrome->setFilterCounts(category->tweakCount(), m_state->appliedCount(*category),
                                  m_state->pendingCount(*category));
    }

    m_tweaks->setSections(sections, emptyMessage);
    m_stack->setCurrentWidget(m_tweakScroll);
}

void MainWindow::refreshCounters()
{
    m_chrome->setPending(m_state->pendingCount());
    refreshOverviewCatalog();

    const Category *category = Catalog::instance().category(m_state->selectedCategory());
    if (Sidebar::isPinnedPage(m_state->selectedCategory()))
        m_chrome->setPendingLabel({});
    else if (m_state->searching())
        m_chrome->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount()));
    else if (category && !category->isOverview())
        m_chrome->setPendingLabel(Locale::tr(QStringLiteral("content.pending")).arg(m_state->pendingCount(*category)));
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
    m_chrome->setSearchText(text);
}

void MainWindow::onCategoryActivated(const QString &id)
{
    // Picking a category is an explicit "show me this", so it leaves the search behind.
    if (m_state->searching())
        m_chrome->clearSearch();

    m_state->setSelectedCategory(id);
    m_chrome->setSelected(id);
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
    return inner.adjusted(0, m_chrome ? m_chrome->titleBarHeight() : 0, 0, 0);
}

void MainWindow::onApply()
{
    if (m_state->pendingCount() == 0 || m_applyOverlay->running())
        return;

    // Settings offers this switch and nothing used to read it, so the confirmation it
    // promises never appeared — Uygula went straight to the writes whichever way it was
    // set. It defaults to on, which is why this is the path most users were missing.
    if (Settings::instance().confirmBeforeApply()) {
        const bool go = Dialog::confirm(
            this,
            Locale::tr(QStringLiteral("mw.confirm.title")).arg(m_state->pendingCount()),
            Locale::tr(QStringLiteral("mw.confirm.body")),
            Locale::tr(QStringLiteral("mw.confirm.apply")),
            Locale::tr(QStringLiteral("mw.confirm.cancel")));
        if (!go)
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
            showNotice(Locale::tr(QStringLiteral("mw.notice.applied")).arg(succeeded));
        return;
    }

    const AppState::ApplyReport report{succeeded, failed, elevationRequired, firstError};

    if (report.elevationRequired && !TweakEngine::isElevated()) {
        // These tweaks live outside HKCU, so they cannot be written by a standard token.
        const bool restart = Dialog::confirm(
            this,
            Locale::tr(QStringLiteral("mw.elevate.title")).arg(report.failed),
            Locale::tr(QStringLiteral("mw.elevate.body")),
            Locale::tr(QStringLiteral("mw.elevate.restart")),
            Locale::tr(QStringLiteral("mw.elevate.later")));

        if (restart) {
            m_state->stashPending();
            if (TweakEngine::relaunchElevated())
                close();
            else
                showNotice(Locale::tr(QStringLiteral("mw.notice.restartCancelled")));
        } else {
            showNotice(Locale::tr(QStringLiteral("mw.notice.elevatePending")).arg(report.failed));
        }
        return;
    }

    showNotice(report.firstError.isEmpty()
                               ? Locale::tr(QStringLiteral("mw.notice.applyFailed")).arg(report.failed)
                               : Locale::tr(QStringLiteral("mw.notice.applyFailedDetail"))
                                     .arg(report.failed).arg(report.firstError));
}

void MainWindow::onRevert()
{
    m_state->revertPending();
    refreshView();
}

