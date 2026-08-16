#include "overviewpage.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/livechart.h"
#include "../widgets/overviewblocks.h"
#include "../widgets/sectionheader.h"

#include <QGridLayout>
#include <QLocale>
#include <QVBoxLayout>

namespace {

constexpr int PadLeft = 18;
constexpr int PadTop = 2;
constexpr int PadRight = 12;
constexpr int PadBottom = 16;

constexpr int TileGap = 8;
constexpr int TilesTopSpace = 4;
// The blocks are cards now and carry their own padding, so the gutters between them
// close up: 28/18 of empty window was reading as a gap in the page, not as breathing room.
constexpr int SectionGapV = 12;
constexpr int SectionGapH = 12;

/// Whole percents read best, but an idle machine sitting at "%0" looks broken — so keep
/// one decimal while the figure is still in single digits.
QString percent(qreal value)
{
    if (value > 0.0 && value < 10.0)
        return QStringLiteral("%%1").arg(QLocale().toString(value, 'f', 1));
    return QStringLiteral("%%1").arg(qRound(value));
}

/// "12,4 GB" / "953 GB" / "820 MB" — one decimal only where it carries information.
QString bytes(quint64 value)
{
    constexpr qreal KB = 1024.0;
    const qreal gb = value / (KB * KB * KB);
    if (gb >= 100.0)
        return QStringLiteral("%1 GB").arg(qRound(gb));
    if (gb >= 1.0)
        return QStringLiteral("%1 GB").arg(QLocale().toString(gb, 'f', 1));
    const qreal mb = value / (KB * KB);
    return QStringLiteral("%1 MB").arg(qRound(mb));
}

/// Capacities read better whole: installed memory is "32 GB", not "31,7 GB".
QString bytesWhole(quint64 value)
{
    constexpr qreal KB = 1024.0;
    return QStringLiteral("%1 GB").arg(qRound(value / (KB * KB * KB)));
}

/// "1,2 MB/s" / "240 kB/s" / "0 B/s"
QString rate(quint64 bytesPerSecond)
{
    constexpr qreal KB = 1024.0;
    if (bytesPerSecond >= KB * KB)
        return QStringLiteral("%1 MB/s").arg(QLocale().toString(bytesPerSecond / (KB * KB), 'f', 1));
    if (bytesPerSecond >= KB)
        return QStringLiteral("%1 kB/s").arg(qRound(bytesPerSecond / KB));
    return QStringLiteral("%1 B/s").arg(bytesPerSecond);
}

/// Peak and mean of a history window, ignoring the samples not taken yet.
QPair<qreal, qreal> peakAndMean(const QVector<qreal> &history)
{
    qreal peak = 0.0;
    qreal sum = 0.0;
    int count = 0;
    for (qreal v : history) {
        if (v < 0.0)
            continue;
        peak = qMax(peak, v);
        sum += v;
        ++count;
    }
    return {peak, count > 0 ? sum / count : 0.0};
}

} // namespace

OverviewPage::OverviewPage(SystemMonitor *monitor, QWidget *parent)
    : QWidget(parent)
    , m_monitor(monitor)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(PadLeft, PadTop, PadRight, PadBottom);
    outer->setSpacing(Theme::Metric::SectionGap);

    // --- live stat tiles ----------------------------------------------------
    auto *tiles = new QWidget(this);
    auto *tileGrid = new QGridLayout(tiles);
    tileGrid->setContentsMargins(0, TilesTopSpace, 0, 0);
    tileGrid->setHorizontalSpacing(TileGap);
    tileGrid->setVerticalSpacing(TileGap);

    m_tileCpu = new StatTile(Locale::tr(QStringLiteral("overview.tile.cpu")), tiles);
    m_tileRam = new StatTile(Locale::tr(QStringLiteral("overview.tile.ram")), tiles);
    m_tileDisk = new StatTile(Locale::tr(QStringLiteral("overview.tile.disk")), tiles);
    m_tileNet = new StatTile(Locale::tr(QStringLiteral("overview.tile.net")), tiles);

    tileGrid->addWidget(m_tileCpu, 0, 0);
    tileGrid->addWidget(m_tileRam, 0, 1);
    tileGrid->addWidget(m_tileDisk, 0, 2);
    tileGrid->addWidget(m_tileNet, 0, 3);
    for (int c = 0; c < 4; ++c)
        tileGrid->setColumnStretch(c, 1);

    outer->addWidget(tiles);

    // --- live chart ---------------------------------------------------------
    auto *chartBlock = new QWidget(this);
    auto *chartLayout = new QVBoxLayout(chartBlock);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->setSpacing(0);

    m_chartHeader = new SectionHeader(Locale::tr(QStringLiteral("overview.chart.title")), chartBlock);
    m_chartHeader->setCount(Locale::tr(QStringLiteral("overview.chart.interval")));
    chartLayout->addWidget(m_chartHeader);

    m_chart = new LiveChart(chartBlock);
    chartLayout->addWidget(m_chart);

    outer->addWidget(chartBlock);

    // --- info blocks --------------------------------------------------------
    auto *sections = new QWidget(this);
    auto *grid = new QGridLayout(sections);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(SectionGapH);
    grid->setVerticalSpacing(SectionGapV);

    m_system = new InfoSection(Locale::tr(QStringLiteral("overview.section.system")), sections);
    m_user = new InfoSection(Locale::tr(QStringLiteral("overview.section.user")), sections);
    m_hardware = new InfoSection(Locale::tr(QStringLiteral("overview.section.hardware")), sections);
    m_display = new InfoSection(Locale::tr(QStringLiteral("overview.section.display")), sections);
    m_network = new InfoSection(Locale::tr(QStringLiteral("overview.section.network")), sections);
    m_power = new InfoSection(Locale::tr(QStringLiteral("overview.section.power")), sections);
    m_security = new InfoSection(Locale::tr(QStringLiteral("overview.section.security")), sections);
    m_storage = new InfoSection(Locale::tr(QStringLiteral("overview.section.storage")), sections);
    m_session = new InfoSection(Locale::tr(QStringLiteral("overview.section.session")), sections);
    m_firmware = new InfoSection(Locale::tr(QStringLiteral("overview.section.firmware")), sections);
    m_processor = new InfoSection(Locale::tr(QStringLiteral("overview.section.processor")), sections);
    m_memory = new InfoSection(Locale::tr(QStringLiteral("overview.section.memory")), sections);
    m_software = new InfoSection(Locale::tr(QStringLiteral("overview.section.software")), sections);
    m_locale = new InfoSection(Locale::tr(QStringLiteral("overview.section.locale")), sections);
    m_processes = new InfoSection(Locale::tr(QStringLiteral("overview.section.processes")), sections);
    m_catalog = new InfoSection(Locale::tr(QStringLiteral("overview.section.catalog")), sections);

    // Three columns rather than the handoff's two: there is a lot to show and the window
    // is wide enough that a third column costs nothing but gains a whole screen of
    // vertical space. Ordered so related blocks sit near each other.
    const QVector<InfoSection *> order{
        m_system,    m_hardware,  m_processor,
        m_user,      m_memory,    m_firmware,
        m_display,   m_storage,   m_network,
        m_security,  m_power,     m_software,
        m_session,   m_processes, m_locale,
        m_catalog,
    };
    for (int i = 0; i < order.size(); ++i)
        grid->addWidget(order.at(i), i / 3, i % 3);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);

    outer->addWidget(sections);
    outer->addStretch(1);

    connect(m_monitor, &SystemMonitor::sampled, this, &OverviewPage::onSampled);
    onSampled(m_monitor->latest());
    setFacts(m_facts);
    setCatalogState(0, 0, 0, QString());   // until MainWindow reports the real numbers

    // The per-second tick in onSampled() already keeps values current; only the static
    // titles need a nudge, and only when the language actually changes.
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        m_tileCpu->setLabel(Locale::tr(QStringLiteral("overview.tile.cpu")));
        m_tileRam->setLabel(Locale::tr(QStringLiteral("overview.tile.ram")));
        m_tileDisk->setLabel(Locale::tr(QStringLiteral("overview.tile.disk")));
        m_tileNet->setLabel(Locale::tr(QStringLiteral("overview.tile.net")));
        m_chartHeader->setTitle(Locale::tr(QStringLiteral("overview.chart.title")));
        m_system->setTitle(Locale::tr(QStringLiteral("overview.section.system")));
        m_user->setTitle(Locale::tr(QStringLiteral("overview.section.user")));
        m_hardware->setTitle(Locale::tr(QStringLiteral("overview.section.hardware")));
        m_display->setTitle(Locale::tr(QStringLiteral("overview.section.display")));
        m_network->setTitle(Locale::tr(QStringLiteral("overview.section.network")));
        m_power->setTitle(Locale::tr(QStringLiteral("overview.section.power")));
        m_security->setTitle(Locale::tr(QStringLiteral("overview.section.security")));
        m_storage->setTitle(Locale::tr(QStringLiteral("overview.section.storage")));
        m_session->setTitle(Locale::tr(QStringLiteral("overview.section.session")));
        m_firmware->setTitle(Locale::tr(QStringLiteral("overview.section.firmware")));
        m_processor->setTitle(Locale::tr(QStringLiteral("overview.section.processor")));
        m_memory->setTitle(Locale::tr(QStringLiteral("overview.section.memory")));
        m_software->setTitle(Locale::tr(QStringLiteral("overview.section.software")));
        m_locale->setTitle(Locale::tr(QStringLiteral("overview.section.locale")));
        m_processes->setTitle(Locale::tr(QStringLiteral("overview.section.processes")));
        m_catalog->setTitle(Locale::tr(QStringLiteral("overview.section.catalog")));
    });
}

void OverviewPage::onSampled(const Sample &sample)
{
    m_tileCpu->setValue(percent(sample.cpuPercent));
    m_tileCpu->setMeter(sample.cpuPercent / 100.0);
    m_tileCpu->setSub(m_monitor->physicalCores() > 0
                          ? Locale::tr(QStringLiteral("ov.cekirdekIsParcacigi"))
                                .arg(m_monitor->physicalCores())
                                .arg(m_monitor->logicalCores())
                          : Locale::tr(QStringLiteral("ov.isParcacigi")).arg(m_monitor->logicalCores()));

    m_tileRam->setValue(percent(sample.ramPercent));
    m_tileRam->setMeter(sample.ramPercent / 100.0);
    m_tileRam->setSub(sample.ramTotal > 0
                          ? QStringLiteral("%1 / %2").arg(bytes(sample.ramUsed), bytesWhole(sample.ramTotal))
                          : QStringLiteral("—"));

    const qreal diskFraction = sample.diskTotal > 0
                                   ? qreal(sample.diskUsed) / qreal(sample.diskTotal)
                                   : 0.0;
    m_tileDisk->setValue(percent(diskFraction * 100.0));
    m_tileDisk->setMeter(diskFraction);
    m_tileDisk->setSub(sample.diskTotal > 0
                           ? Locale::tr(QStringLiteral("ov.bos"))
                                 .arg(bytes(sample.diskTotal - sample.diskUsed), bytes(sample.diskTotal))
                           : QStringLiteral("—"));

    m_tileNet->setValue(rate(sample.downBytesPerSec));
    m_tileNet->setSub(Locale::tr(QStringLiteral("ov.gonderme")).arg(rate(sample.upBytesPerSec)));

    m_chart->setSeries(m_monitor->cpuHistory(), m_monitor->ramHistory(), SystemMonitor::HistorySize);

    // What the plot cannot say on its own: how high it went and where it sat.
    const auto cpu = peakAndMean(m_monitor->cpuHistory());
    const auto ram = peakAndMean(m_monitor->ramHistory());
    m_chartHeader->setCount(Locale::tr(QStringLiteral("overview.chart.summary"))
                                .arg(percent(cpu.first), percent(cpu.second), percent(ram.second)));

    // Uptime and the process counters are the rows that move on their own.
    m_session->setRowValue(0, SysInfo::uptimeString());

    const SysInfo::LiveCounters live = SysInfo::liveCounters();
    m_processes->setRowValue(0, live.processes);
    m_processes->setRowValue(1, live.threads);
    m_processes->setRowValue(2, live.handles);
    m_processes->setRowValue(3, live.idle);
}

void OverviewPage::setFacts(const SysInfo::Facts &facts)
{
    m_facts = facts;

    // Mono vs. text per row follows the mockup's m() / pl() split exactly.
    m_system->setRows({
        {Locale::tr(QStringLiteral("ov.isletimSistemi")), facts.osName, false},
        {Locale::tr(QStringLiteral("ov.surum")), facts.version, true},
        {Locale::tr(QStringLiteral("ov.yuklemeTarihi")), facts.installDate, true},
        {Locale::tr(QStringLiteral("ov.sonGuncelleme")), facts.lastUpdate, true},
        {Locale::tr(QStringLiteral("ov.etkinlestirme")), facts.activation, false},
        {Locale::tr(QStringLiteral("ov.guvenliOnyukleme")), facts.secureBoot, false},
        {QStringLiteral("TPM"), facts.tpm, true},
        {Locale::tr(QStringLiteral("ov.surumDali")), facts.buildBranch, true},
        {Locale::tr(QStringLiteral("ov.edisyonKimligi")), facts.editionId, true},
        {Locale::tr(QStringLiteral("ov.windowsDizini")), facts.windowsDir, true},
        {Locale::tr(QStringLiteral("ov.sistemSurucusu")), facts.systemDrive, true},
    });

    m_user->setRows({
        {Locale::tr(QStringLiteral("ov.kullaniciAdi")), facts.userName, true},
        {Locale::tr(QStringLiteral("ov.hesapTuru")), facts.accountType, false},
        {Locale::tr(QStringLiteral("ov.bilgisayarAdi")), facts.computerName, true},
        {Locale::tr(QStringLiteral("ov.microsoftHesabi")), facts.microsoftAccount, false},
        {Locale::tr(QStringLiteral("ov.etkinGrupIlkesi")), facts.activePolicies, true},
        {Locale::tr(QStringLiteral("ov.profilKlasoru")), facts.profilePath, true},
    });

    m_hardware->setRows({
        {Locale::tr(QStringLiteral("ov.islemci")), facts.cpu, false},
        {Locale::tr(QStringLiteral("ov.bellek")), facts.memory, false},
        {Locale::tr(QStringLiteral("ov.ekranKarti")), facts.gpu, false},
        {Locale::tr(QStringLiteral("ov.depolama")), facts.storage, false},
        {Locale::tr(QStringLiteral("ov.anakart")), facts.motherboard, false},
        {QStringLiteral("BIOS"), facts.bios, true},
    });

    m_display->setRows({
        {Locale::tr(QStringLiteral("ov.ekranSayisi")), facts.displayCount, false},
        {Locale::tr(QStringLiteral("ov.cozunurluk")), facts.resolution, true},
        {Locale::tr(QStringLiteral("ov.renkDerinligi")), facts.colorDepth, true},
        {Locale::tr(QStringLiteral("ov.olcekleme")), facts.dpiScale, true},
        {Locale::tr(QStringLiteral("ov.ekranSurucusu")), facts.graphicsDriver, true},
        {Locale::tr(QStringLiteral("ov.sanalMasaustu")), facts.virtualDesktop, true},
    });

    m_network->setRows({
        {Locale::tr(QStringLiteral("ov.bagdastirici")), facts.adapter, false},
        {Locale::tr(QStringLiteral("ov.ipv4Adresi")), facts.ipv4, true},
        {Locale::tr(QStringLiteral("ov.baglantiHizi")), facts.linkSpeed, true},
        {Locale::tr(QStringLiteral("ov.dnsSunucusu")), facts.dnsServer, true},
        {Locale::tr(QStringLiteral("ov.ipv6Adresi")), facts.ipv6, true},
        {Locale::tr(QStringLiteral("ov.agGecidi")), facts.gateway, true},
        {Locale::tr(QStringLiteral("ov.bagdastirici")), facts.adapterCount, false},
        {Locale::tr(QStringLiteral("ov.macAdresi")), facts.macAddress, true},
        {Locale::tr(QStringLiteral("ov.etkiAlani")), facts.domain, false},
    });

    m_power->setRows({
        {Locale::tr(QStringLiteral("ov.gucKaynagi")), facts.powerSource, false},
        {Locale::tr(QStringLiteral("ov.pil")), facts.battery, false},
        {Locale::tr(QStringLiteral("ov.gucPlani")), facts.powerPlan, false},
    });

    m_security->setRows({
        {QStringLiteral("Microsoft Defender"), facts.defender, false},
        {Locale::tr(QStringLiteral("ov.guvenlikDuvari")), facts.firewall, false},
        {QStringLiteral("SmartScreen"), facts.smartScreen, false},
        {Locale::tr(QStringLiteral("ov.cekirdekYalitimi")), facts.coreIsolation, false},
        {Locale::tr(QStringLiteral("ov.sanallastirmaGuvenligi")), facts.virtualization, false},
    });

    QVector<InfoRow> volumes;
    volumes.reserve(facts.volumes.size());
    for (const SysInfo::Facts::Volume &volume : facts.volumes)
        volumes.append({volume.name, volume.detail, true, volume.used});
    if (volumes.isEmpty())
        volumes.append({Locale::tr(QStringLiteral("ov.birim")), QStringLiteral("—"), false, -1.0});
    m_storage->setRows(volumes);
    m_storage->setNote(Locale::tr(QStringLiteral("ov.birimSayisi")).arg(facts.volumes.size()));

    m_firmware->setRows({
        {Locale::tr(QStringLiteral("ov.uretici")), facts.manufacturer, false},
        {Locale::tr(QStringLiteral("ov.model")), facts.model, false},
        {Locale::tr(QStringLiteral("ov.anakart")), facts.motherboard, false},
        {Locale::tr(QStringLiteral("ov.biosUreticisi")), facts.biosVendor, false},
        {Locale::tr(QStringLiteral("ov.biosTarihi")), facts.biosDate, true},
        {QStringLiteral("SMBIOS"), facts.smbios, true},
        {Locale::tr(QStringLiteral("ov.onyuklemeModu")), facts.bootMode, false},
    });

    m_processor->setRows({
        {Locale::tr(QStringLiteral("ov.model")), facts.cpu, false},
        {Locale::tr(QStringLiteral("ov.uretici")), facts.cpuVendor, true},
        {Locale::tr(QStringLiteral("ov.tabanFrekans")), facts.cpuBaseClock, true},
        {Locale::tr(QStringLiteral("ov.mimari")), facts.cpuArchitecture, true},
        {Locale::tr(QStringLiteral("ov.sanallastirma")), facts.cpuVirtualization, false},
    });

    m_memory->setRows({
        {Locale::tr(QStringLiteral("ov.takili")), facts.memory, false},
        {Locale::tr(QStringLiteral("ov.kullanimda")), facts.memoryInUse, true},
        {Locale::tr(QStringLiteral("ov.bosta")), facts.memoryFree, true},
        {Locale::tr(QStringLiteral("ov.sayfaDosyasi")), facts.pageFile, true},
        {Locale::tr(QStringLiteral("ov.yuvalar")), facts.memorySlots, true},
    });

    m_software->setRows({
        {Locale::tr(QStringLiteral("ov.kuruluProgram")), facts.installedPrograms, true},
        {Locale::tr(QStringLiteral("ov.baslangicGirisi")), facts.startupEntries, true},
        {QStringLiteral(".NET Framework"), facts.dotNet, true},
        {QStringLiteral("PowerShell"), facts.powerShell, true},
        {Locale::tr(QStringLiteral("ov.varsayilanTarayici")), facts.defaultBrowser, false},
    });

    m_locale->setRows({
        {Locale::tr(QStringLiteral("ov.saatDilimi")), facts.timeZone, false},
        {Locale::tr(QStringLiteral("ov.ntpSunucusu")), facts.ntpServer, true},
        {Locale::tr(QStringLiteral("ov.dilVeBolge")), facts.locale, false},
        {Locale::tr(QStringLiteral("ov.klavyeDuzeni")), facts.keyboardLayout, true},
    });

    m_processes->setRows({
        {Locale::tr(QStringLiteral("ov.calisanSurec")), facts.processCount, true},
        {Locale::tr(QStringLiteral("ov.isParcacigi2")), facts.threadCount, true},
        {Locale::tr(QStringLiteral("ov.tanitici")), facts.handleCount, true},
        {Locale::tr(QStringLiteral("ov.bostaSure")), facts.idleTime, true},
    });

    m_session->setRows({
        {Locale::tr(QStringLiteral("ov.calismaSuresi")), facts.uptime, true},
        {Locale::tr(QStringLiteral("ov.sonOnyukleme")), facts.lastBoot, true},
        {Locale::tr(QStringLiteral("ov.sonOturumAcma")), facts.lastLogon, true},
        {Locale::tr(QStringLiteral("ov.bekleyenYenidenBaslatma")), facts.pendingRestart, false},
        {Locale::tr(QStringLiteral("ov.sonGeriYuklemeNoktasi")), facts.lastRestorePoint, true},
    });
}

void OverviewPage::setCatalogState(int total, int applied, int pending, const QString &busiest)
{
    const qreal share = total > 0 ? qreal(applied) / qreal(total) : -1.0;

    m_catalog->setRows({
        {Locale::tr(QStringLiteral("ov.katalog")), Locale::tr(QStringLiteral("ov.tweakSayisi")).arg(total), true, -1.0},
        {Locale::tr(QStringLiteral("ov.etkin")), total > 0 ? QStringLiteral("%1 · %%2").arg(applied)
                                                  .arg(qRound(100.0 * share))
                                            : QStringLiteral("—"), true, share},
        {Locale::tr(QStringLiteral("ov.bekleyenDegisiklik")), QString::number(pending), true, -1.0},
        {Locale::tr(QStringLiteral("ov.enCokDegisen")), busiest.isEmpty() ? QStringLiteral("—") : busiest, false, -1.0},
        {Locale::tr(QStringLiteral("ov.yazmaYetkisi")),
         SysInfo::elevationLabel(m_facts.elevated), false, -1.0},
    });
}
