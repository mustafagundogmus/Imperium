// overviewpage.h — the Genel Bakış screen.
//
//   stat tiles   4-column grid of cards, gap 8px, each with a usage meter
//   live chart   section header (carrying the 60-second peak and mean) + rolling plot
//   info blocks  3-column grid of cards, gap 12px
//
// The tiles and the chart are driven by SystemMonitor; the info blocks come from the
// one-shot SysInfo read, whatever the background probe fills in later, and — for the
// catalogue block — from AppState, because how much of the machine this app has actually
// changed belongs on the page that describes the machine.

#pragma once

#include "../deepinfo.h"
#include "../monitor.h"
#include "../sysinfo.h"

#include <QWidget>

class InfoSection;
class LiveChart;
class SectionHeader;
class StatTile;

class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(SystemMonitor *monitor, QWidget *parent = nullptr);

    void setFacts(const SysInfo::Facts &facts);

    /// The blocks the deep probe feeds. Called once per stage as each lands, so the
    /// twelve cards below fill in behind the ones SysInfo could answer immediately
    /// rather than all appearing at once several seconds in.
    void setDeepFacts(const DeepInfo::Facts &facts);

    /// Catalogue state, pushed by MainWindow whenever it changes.
    void setCatalogState(int total, int applied, int pending, const QString &busiest);

private Q_SLOTS:
    void onSampled(const Sample &sample);

private:
    SystemMonitor *m_monitor = nullptr;
    SysInfo::Facts m_facts;
    SectionHeader *m_chartHeader = nullptr;

    StatTile *m_tileCpu = nullptr;
    StatTile *m_tileRam = nullptr;
    StatTile *m_tileDisk = nullptr;
    StatTile *m_tileNet = nullptr;
    LiveChart *m_chart = nullptr;

    InfoSection *m_system = nullptr;
    InfoSection *m_user = nullptr;
    InfoSection *m_hardware = nullptr;
    InfoSection *m_display = nullptr;
    InfoSection *m_network = nullptr;
    InfoSection *m_power = nullptr;
    InfoSection *m_security = nullptr;
    InfoSection *m_storage = nullptr;
    InfoSection *m_session = nullptr;
    InfoSection *m_firmware = nullptr;
    InfoSection *m_processor = nullptr;
    InfoSection *m_memory = nullptr;
    InfoSection *m_software = nullptr;
    InfoSection *m_locale = nullptr;
    InfoSection *m_processes = nullptr;
    InfoSection *m_catalog = nullptr;

    // Fed by DeepInfo::Probe rather than by SysInfo::collect().
    InfoSection *m_update = nullptr;
    InfoSection *m_integrity = nullptr;
    InfoSection *m_tasks = nullptr;
    InfoSection *m_drivers = nullptr;
    InfoSection *m_privacy = nullptr;
    InfoSection *m_encryption = nullptr;
    InfoSection *m_accounts = nullptr;
    InfoSection *m_virtualisation = nullptr;
    InfoSection *m_diskHealth = nullptr;
    InfoSection *m_performance = nullptr;
    InfoSection *m_connection = nullptr;
    InfoSection *m_sensors = nullptr;

    DeepInfo::Facts m_deep;
};
