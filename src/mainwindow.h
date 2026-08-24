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

#pragma once

#include "framelesswindow.h"
#include "monitor.h"
#include "sysinfo.h"

#include <QDateTime>
#include <QVector>

class AboutPage;
class AppState;
class ContentHeader;
class DebloatPage;
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

    /// Opens the search with  text already in it — the command line's --search, which
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

private:
    void buildUi();
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
    SystemMonitor *m_monitor = nullptr;
    QDateTime m_scannedAt;

    TitleBar *m_titleBar = nullptr;
    Sidebar *m_sidebar = nullptr;
    ContentHeader *m_header = nullptr;
    QStackedWidget *m_stack = nullptr;
    SmoothScrollArea *m_overviewScroll = nullptr;
    SmoothScrollArea *m_tweakScroll = nullptr;
    SmoothScrollArea *m_settingsScroll = nullptr;
    SmoothScrollArea *m_actionScroll = nullptr;
    SmoothScrollArea *m_debloatScroll = nullptr;
    SmoothScrollArea *m_journalScroll = nullptr;
    SmoothScrollArea *m_aboutScroll = nullptr;
    SettingsPage *m_settings = nullptr;
    ActionPage *m_actions = nullptr;
    DebloatPage *m_debloat = nullptr;
    JournalPage *m_journal = nullptr;
    AboutPage *m_about = nullptr;
    Updater *m_updater = nullptr;
    OverviewPage *m_overview = nullptr;
    TweakPage *m_tweaks = nullptr;
    StatusBar *m_statusBar = nullptr;
    ApplyOverlay *m_applyOverlay = nullptr;

    bool m_alphabetical = false;
};
