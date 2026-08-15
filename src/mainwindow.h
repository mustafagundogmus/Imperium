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

class AppState;
class ContentHeader;
class OverviewPage;
class QStackedWidget;
class Sidebar;
class SmoothScrollArea;
class StatusBar;
class TitleBar;
class ApplyOverlay;
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

private Q_SLOTS:
    void onCategoryActivated(const QString &id);
    void onQueryChanged(const QString &query);
    void onFilterChanged(int index);
    void onSortToggled(bool alphabetical);
    void onApply();
    void onApplyFinished(int succeeded, int failed, bool elevationRequired);
    void onRevert();
    void onRestorePointRequested();

private:
    void buildUi();
    void wire();
    void refreshView();       ///< header + list + stack page
    void refreshCounters();   ///< pending-driven labels and tiles
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
    SettingsPage *m_settings = nullptr;
    Updater *m_updater = nullptr;
    OverviewPage *m_overview = nullptr;
    TweakPage *m_tweaks = nullptr;
    StatusBar *m_statusBar = nullptr;
    ApplyOverlay *m_applyOverlay = nullptr;

    bool m_alphabetical = false;
};
