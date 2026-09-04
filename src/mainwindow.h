// mainwindow.h — composition root.
//
// Window (1060×820, 1px border)
//   ├── TitleBar                    36px
//   └── content row                 flex
//        ├── Sidebar                212px, full height of the row
//        └── main column            flex
//             ├── ContentHeader
//             ├── scroll stack      flex — Genel Bakış | tweak list
//             └── StatusBar         36px
//
// That is the classic shell. Since 0.14.0 the four widgets around the stack are one
// Chrome, and there are two of them — ClassicChrome above, FluentChrome for the Windows
// 11 layout — chosen from the settings page and swapped live: the pages stay, the chrome
// around them is rebuilt. See views/chrome.h.

#pragma once

#include "deepinfo.h"
#include "framelesswindow.h"
#include "monitor.h"
#include "sysinfo.h"

#include <QDateTime>
#include <QVector>

class AboutPage;
class AppsPage;
class AppState;
class FeaturesPage;
class Chrome;
class CleanerPage;
class DebloatPage;
class GodModePage;
class OverviewPage;
class QStackedWidget;
class Sidebar;
class SmoothScrollArea;
class StatusBar;
class TitleBar;
class ApplyOverlay;
class ActionPage;
class JournalPage;
class SettingsPage;
class TiLauncherPage;
class TweakEngine;
class TweakPage;
class Updater;
struct Section;

class MainWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    /// Opens a category by id ("ov", "priv", …). Ignored if the id is unknown.
    void showCategory(const QString &id);

    /// Opens the search with \a text already in it — the command line's --search, which
    /// is how a support answer can say "run it like this and look at what comes up".
    void showSearch(const QString &text);

private Q_SLOTS:
    void onCategoryActivated(const QString &id);
    void onQueryChanged(const QString &query);
    void onFilterChanged(int index);
    void onSortToggled(bool alphabetical);
    void onApply();
    void onApplyFinished(int succeeded, int failed, bool elevationRequired,
                         const QString &firstError);
    void onRevert();

protected:
    // The apply overlay is a hand-placed child of the card rather than a layout item, so
    // its geometry only follows the window if something puts it there. Both handlers are
    // needed: a drag-resize arrives as resizeEvent, a maximise or restore as a
    // WindowStateChange whose resize lands before the state has settled.
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    void buildUi();
    /// Builds the shell Theme::shell() names around the page stack, tearing down the one
    /// before it. Called once from buildUi() and again on every shell switch.
    void buildChrome();
    void wireChrome();
    /// The card's minimum and opening size, which differ per shell.
    void applyShellMetrics();
    /// A few seconds of text in the shell's status line, whichever shell it is.
    void showNotice(const QString &text);
    void syncOverlayGeometry();
    void wire();
    void refreshView();       ///< header + list + stack page
    void refreshCounters();   ///< pending-driven labels and tiles
    void refreshOverviewCatalog();   ///< the Genel Bakış block that counts applied tweaks
    QVector<Section> visibleSections() const;
    QRect overlayRect() const;

    TweakEngine *m_engine = nullptr;
    AppState *m_state = nullptr;
    SysInfo::Facts m_facts;
    SysInfo::Probe *m_probe = nullptr;
    DeepInfo::Probe *m_deepProbe = nullptr;
    SystemMonitor *m_monitor = nullptr;
    QDateTime m_scannedAt;

    Chrome *m_chrome = nullptr;
    QStackedWidget *m_stack = nullptr;
    SmoothScrollArea *m_overviewScroll = nullptr;
    SmoothScrollArea *m_tweakScroll = nullptr;
    SmoothScrollArea *m_settingsScroll = nullptr;
    SmoothScrollArea *m_actionScroll = nullptr;
    SmoothScrollArea *m_tiScroll = nullptr;
    SmoothScrollArea *m_debloatScroll = nullptr;
    SmoothScrollArea *m_cleanerScroll = nullptr;
    SmoothScrollArea *m_godModeScroll = nullptr;
    SmoothScrollArea *m_journalScroll = nullptr;
    SmoothScrollArea *m_aboutScroll = nullptr;
    SmoothScrollArea *m_appsScroll = nullptr;
    SmoothScrollArea *m_featuresScroll = nullptr;
    SettingsPage *m_settings = nullptr;
    ActionPage *m_actions = nullptr;
    TiLauncherPage *m_tiLauncher = nullptr;
    DebloatPage *m_debloat = nullptr;
    CleanerPage *m_cleaner = nullptr;
    GodModePage *m_godMode = nullptr;
    JournalPage *m_journal = nullptr;
    AboutPage *m_about = nullptr;
    AppsPage *m_apps = nullptr;
    FeaturesPage *m_features = nullptr;
    Updater *m_updater = nullptr;
    OverviewPage *m_overview = nullptr;
    TweakPage *m_tweaks = nullptr;
    ApplyOverlay *m_applyOverlay = nullptr;

    bool m_alphabetical = false;

    /// An update offer is on screen or about to be. A modal dialog runs its own event
    /// loop, so a launch-time check that completes while the user is reading the offer
    /// would otherwise stack a second one on top of the first.
    bool m_offering = false;
};
