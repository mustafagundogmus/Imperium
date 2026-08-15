// overviewpage.h — the Genel Bakış screen.
//
//   stat tiles   4-column grid, gap 8px, 4px of space above
//   live chart   section header + rolling CPU/memory plot
//   info blocks  2-column grid, gap 18px vertical / 28px horizontal
//
// All four tiles and the chart are driven by SystemMonitor; the info blocks come from the
// one-shot SysInfo read plus whatever the background probe fills in later.

#pragma once

#include "../monitor.h"
#include "../sysinfo.h"

#include <QWidget>

class InfoSection;
class LiveChart;
class StatTile;

class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(SystemMonitor *monitor, QWidget *parent = nullptr);

    void setFacts(const SysInfo::Facts &facts);

private Q_SLOTS:
    void onSampled(const Sample &sample);

private:
    SystemMonitor *m_monitor = nullptr;
    SysInfo::Facts m_facts;

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
};
