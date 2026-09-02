#include "overviewpage.h"
#include "../deepinfo.h"
#include "../i18n.h"
#include "../icons.h"
#include "../theme.h"
#include "../widgets/livechart.h"
#include "../widgets/overviewblocks.h"
#include "../widgets/sectionheader.h"

#include <QGridLayout>
#include <QLocale>
#include <QVBoxLayout>

namespace {

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
    outer->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                              Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
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

    // Each card's glyph, from lucide (see the block at the bottom of icons.h). The pairing
    // is the point of this list, so it is written out here rather than hidden behind a
    // lookup: a card whose title says one thing and whose glyph says another is a bug you
    // can only see by reading the two side by side, which is what this is.
    //
    // Three of them are chips and they are three different chips on purpose: the square
    // die with pins on four sides for the processor, the vertical DIP package for the
    // firmware that lives on one, and the populated board for the machine as a whole.
    //
    // An alias rather than `using namespace Icons`, which would also drop this file's
    // scope on the free functions next to it — `search` and `sort` among them.
    namespace Lucide = Icons::Lucide;
    m_system = new InfoSection(Locale::tr(QStringLiteral("overview.section.system")),
                               Lucide::AppWindow, sections);
    m_user = new InfoSection(Locale::tr(QStringLiteral("overview.section.user")),
                             Lucide::User, sections);
    m_hardware = new InfoSection(Locale::tr(QStringLiteral("overview.section.hardware")),
                                 Lucide::CircuitBoard, sections);
    m_display = new InfoSection(Locale::tr(QStringLiteral("overview.section.display")),
                                Lucide::Monitor, sections);
    m_network = new InfoSection(Locale::tr(QStringLiteral("overview.section.network")),
                                Lucide::Network, sections);
    m_power = new InfoSection(Locale::tr(QStringLiteral("overview.section.power")),
                              Lucide::BatteryCharging, sections);
    m_security = new InfoSection(Locale::tr(QStringLiteral("overview.section.security")),
                                 Lucide::ShieldCheck, sections);
    m_storage = new InfoSection(Locale::tr(QStringLiteral("overview.section.storage")),
                                Lucide::HardDrive, sections);
    m_session = new InfoSection(Locale::tr(QStringLiteral("overview.section.session")),
                                Lucide::Clock, sections);
    m_firmware = new InfoSection(Locale::tr(QStringLiteral("overview.section.firmware")),
                                 Lucide::Microchip, sections);
    m_processor = new InfoSection(Locale::tr(QStringLiteral("overview.section.processor")),
                                  Lucide::Cpu, sections);
    m_memory = new InfoSection(Locale::tr(QStringLiteral("overview.section.memory")),
                               Lucide::MemoryStick, sections);
    m_software = new InfoSection(Locale::tr(QStringLiteral("overview.section.software")),
                                 Lucide::Package, sections);
    m_locale = new InfoSection(Locale::tr(QStringLiteral("overview.section.locale")),
                               Lucide::Globe, sections);
    m_processes = new InfoSection(Locale::tr(QStringLiteral("overview.section.processes")),
                                  Lucide::Layers, sections);
    m_catalog = new InfoSection(Locale::tr(QStringLiteral("overview.section.catalog")),
                                Lucide::SlidersHorizontal, sections);

    m_update = new InfoSection(Locale::tr(QStringLiteral("overview.section.update")),
                               Lucide::Download, sections);
    m_integrity = new InfoSection(Locale::tr(QStringLiteral("overview.section.integrity")),
                                  Lucide::TriangleAlert, sections);
    m_tasks = new InfoSection(Locale::tr(QStringLiteral("overview.section.tasks")),
                              Lucide::CalendarClock, sections);
    m_drivers = new InfoSection(Locale::tr(QStringLiteral("overview.section.drivers")),
                                Lucide::Plug, sections);
    m_privacy = new InfoSection(Locale::tr(QStringLiteral("overview.section.privacy")),
                                Lucide::EyeOff, sections);
    m_encryption = new InfoSection(Locale::tr(QStringLiteral("overview.section.encryption")),
                                   Lucide::Lock, sections);
    m_accounts = new InfoSection(Locale::tr(QStringLiteral("overview.section.accounts")),
                                 Lucide::Users, sections);
    m_virtualisation = new InfoSection(Locale::tr(QStringLiteral("overview.section.virtualisation")),
                                       Lucide::Box, sections);
    m_diskHealth = new InfoSection(Locale::tr(QStringLiteral("overview.section.diskhealth")),
                                   Lucide::HeartPulse, sections);
    m_performance = new InfoSection(Locale::tr(QStringLiteral("overview.section.performance")),
                                    Lucide::Gauge, sections);
    m_connection = new InfoSection(Locale::tr(QStringLiteral("overview.section.connection")),
                                   Lucide::Wifi, sections);
    m_sensors = new InfoSection(Locale::tr(QStringLiteral("overview.section.sensors")),
                                Lucide::Thermometer, sections);
    m_graphics = new InfoSection(Locale::tr(QStringLiteral("overview.section.graphics")),
                                 Lucide::Gauge, sections);
    m_protection = new InfoSection(Locale::tr(QStringLiteral("overview.section.protection")),
                                   Lucide::ShieldCheck, sections);

    // Three columns rather than the handoff's two: there is a lot to show and the window
    // is wide enough that a third column costs nothing but gains a whole screen of
    // vertical space. Ordered so related blocks sit near each other.
    // Ordered by what a reader is most likely to have come for, three to a row and
    // related blocks side by side: the machine's identity first, then what it is doing,
    // then the things you go looking for when something is wrong.
    const QVector<InfoSection *> order{
        m_system,      m_hardware,       m_processor,
        m_user,        m_memory,         m_firmware,
        m_display,     m_storage,        m_network,
        m_security,    m_protection,     m_privacy,
        m_accounts,    m_update,         m_integrity,
        m_drivers,     m_encryption,     m_virtualisation,
        m_tasks,       m_diskHealth,     m_performance,
        m_connection,  m_power,          m_sensors,
        m_graphics,    m_software,       m_session,
        m_processes,   m_locale,         m_catalog,
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
    setDeepFacts(m_deep);   // every row at "—" until the probe reports
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
        m_update->setTitle(Locale::tr(QStringLiteral("overview.section.update")));
        m_integrity->setTitle(Locale::tr(QStringLiteral("overview.section.integrity")));
        m_tasks->setTitle(Locale::tr(QStringLiteral("overview.section.tasks")));
        m_drivers->setTitle(Locale::tr(QStringLiteral("overview.section.drivers")));
        m_privacy->setTitle(Locale::tr(QStringLiteral("overview.section.privacy")));
        m_encryption->setTitle(Locale::tr(QStringLiteral("overview.section.encryption")));
        m_accounts->setTitle(Locale::tr(QStringLiteral("overview.section.accounts")));
        m_virtualisation->setTitle(Locale::tr(QStringLiteral("overview.section.virtualisation")));
        m_diskHealth->setTitle(Locale::tr(QStringLiteral("overview.section.diskhealth")));
        m_performance->setTitle(Locale::tr(QStringLiteral("overview.section.performance")));
        m_connection->setTitle(Locale::tr(QStringLiteral("overview.section.connection")));
        m_sensors->setTitle(Locale::tr(QStringLiteral("overview.section.sensors")));
        m_graphics->setTitle(Locale::tr(QStringLiteral("overview.section.graphics")));
        m_protection->setTitle(Locale::tr(QStringLiteral("overview.section.protection")));

        // The row labels are rebuilt from the same call that fills them, so a language
        // switch replays both rather than leaving twelve blocks in the old language.
        setDeepFacts(m_deep);
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

    // The Donanım and Ekran blocks grow a row per display adapter, but only on a machine
    // that has more than one. That is the fix: 0.9.10 put one card's name in this block and
    // another card's driver version in that one, two cards apart on the page, where nothing
    // ever brought the two close enough to look wrong. One row per adapter, each row
    // carrying only that adapter's own fields, makes the pair structurally incapable of
    // describing different cards.
    //
    // A machine with one adapter keeps exactly the two rows 0.9.10 drew, from facts.gpu and
    // facts.graphicsDriver — which SysInfo now derives from that same single adapter, so
    // they agree by construction rather than by luck. Drawing the list unconditionally would
    // have cost every single-GPU desktop a card name promoted to a row title and a
    // "Kullanımda" marker that distinguishes the only card from nothing at all. There is
    // no second card to tell it apart from, so there is nothing for the list to say.
    const bool severalAdapters = facts.adapters.size() > 1;

    // The list uses the stacked row, with the adapter's name as the row's title and its
    // values underneath — the shape InfoRow::stacked documents, and the one the storage
    // block already uses for a disk and its details. That is not a preference, it is what
    // fits. An info card gives a row 296 px at the 1240 px minimum window, counted right
    // through: 1240 less the card's two 1 px borders, the 212 px sidebar and the scroll
    // bar's track leaves the page 1014 px; less its 18/12 padding and the grid's two 12 px
    // gutters that is 960 px over three columns, and each column spends 12 px of padding a
    // side. Measured in the real faces, "NVIDIA GeForce RTX 5070 Laptop GPU" is 268 px on
    // its own line and its driver and date another 192 px, while putting the name and the
    // driver on one line comes to 488 px and loses the half that matters.
    QVector<InfoRow> hardware{
        {Locale::tr(QStringLiteral("ov.islemci")), facts.cpu, false},
        {Locale::tr(QStringLiteral("ov.bellek")), facts.memory, false},
    };
    if (!severalAdapters) {
        // One adapter, or none enumerated at all — a non-Windows build, or a registry that
        // would not answer, in which case facts.gpu is still the "—" it starts as.
        hardware.append({Locale::tr(QStringLiteral("ov.ekranKarti")), facts.gpu, false});
    } else {
        for (const SysInfo::Facts::Adapter &adapter : facts.adapters) {
            // "Ekran kartı" leads the value line rather than labelling the row, because the
            // row's title line is spent on the card's name. It also guarantees the line is
            // never empty, which an adapter reporting neither VRAM nor desktop duty would
            // otherwise leave it. 212 px at its longest, so all three parts fit.
            QStringList parts{Locale::tr(QStringLiteral("ov.ekranKarti"))};
            if (!adapter.memory.isEmpty())
                parts << adapter.memory;
            if (adapter.active)
                parts << Locale::tr(QStringLiteral("ov.kullanimda"));
            hardware.append({adapter.name, parts.join(QStringLiteral(" · ")), false, -1.0, true});
        }
    }
    hardware.append({Locale::tr(QStringLiteral("ov.depolama")), facts.storage, false});
    hardware.append({Locale::tr(QStringLiteral("ov.anakart")), facts.motherboard, false});
    hardware.append({QStringLiteral("BIOS"), facts.bios, true});
    m_hardware->setRows(hardware);

    QVector<InfoRow> display{
        {Locale::tr(QStringLiteral("ov.ekranSayisi")), facts.displayCount, false},
        {Locale::tr(QStringLiteral("ov.cozunurluk")), facts.resolution, true},
        {Locale::tr(QStringLiteral("ov.renkDerinligi")), facts.colorDepth, true},
        {Locale::tr(QStringLiteral("ov.olcekleme")), facts.dpiScale, true},
    };
    if (!severalAdapters) {
        // The 0.9.10 row, and now truthful: facts.graphicsDriver is the driver of the very
        // adapter facts.gpu names over in the Donanım block, which is the pairing 0.9.10
        // got from two unrelated registry reads and got wrong.
        display.append({Locale::tr(QStringLiteral("ov.ekranSurucusu")), facts.graphicsDriver, true});
    } else {
        for (const SysInfo::Facts::Adapter &adapter : facts.adapters) {
            // The version and date alone, in the mono face 0.9.10 drew them in, under the
            // name of the card they belong to. No "Ekran sürücüsü" in front of them: that
            // measures 328 px against the row's 296 and would cost the driver date, and
            // this block is already titled "Ekran" with the card named directly above.
            // A card whose driver version is missing still gets its row — that the card is
            // there is worth more than leaving it out for want of a version.
            display.append({adapter.name,
                            adapter.driver.isEmpty() ? QStringLiteral("—") : adapter.driver,
                            true, -1.0, true});
        }
    }
    display.append({Locale::tr(QStringLiteral("ov.sanalMasaustu")), facts.virtualDesktop, true});
    m_display->setRows(display);

    m_network->setRows({
        {Locale::tr(QStringLiteral("ov.bagdastirici")), facts.adapter, false},
        {Locale::tr(QStringLiteral("ov.ipv4Adresi")), facts.ipv4, true},
        {Locale::tr(QStringLiteral("ov.baglantiHizi")), facts.linkSpeed, true},
        {Locale::tr(QStringLiteral("ov.dnsSunucusu")), facts.dnsServer, true},
        {Locale::tr(QStringLiteral("ov.ipv6Adresi")), facts.ipv6, true},
        {Locale::tr(QStringLiteral("ov.agGecidi")), facts.gateway, true},
        {Locale::tr(QStringLiteral("ov.bagdastiriciSayisi")), facts.adapterCount, false},
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
        volumes.append({volume.name, volume.detail, true, volume.used, true});
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

void OverviewPage::setDeepFacts(const DeepInfo::Facts &facts)
{
    m_deep = facts;

    m_update->setRows({
        {Locale::tr(QStringLiteral("deep.pending")), facts.updatePending, false},
        {Locale::tr(QStringLiteral("deep.lastCheck")), facts.updateLastCheck, true},
        {Locale::tr(QStringLiteral("deep.paused")), facts.updatePaused, false},
        {Locale::tr(QStringLiteral("deep.channel")), facts.updateChannel, false},
        {Locale::tr(QStringLiteral("deep.service")), facts.updateService, false},
    });

    m_integrity->setRows({
        {Locale::tr(QStringLiteral("deep.lastCrash")), facts.lastCrash, true, -1.0, true},
        {Locale::tr(QStringLiteral("deep.criticalEvents")), facts.criticalEvents, false},
        {Locale::tr(QStringLiteral("deep.restartReason")), facts.restartReason, false},
        {Locale::tr(QStringLiteral("deep.minidumps")), facts.minidumps, true},
    });

    m_tasks->setRows({
        {Locale::tr(QStringLiteral("deep.taskTotal")), facts.taskTotal, true},
        {Locale::tr(QStringLiteral("deep.taskDisabled")), facts.taskDisabled, true},
        {Locale::tr(QStringLiteral("deep.taskTelemetry")), facts.taskTelemetry, true},
        {Locale::tr(QStringLiteral("deep.taskThirdParty")), facts.taskThirdParty, true},
    });

    m_drivers->setRows({
        {Locale::tr(QStringLiteral("deep.driverProblem")), facts.driverProblem, false, -1.0, true},
        {Locale::tr(QStringLiteral("deep.driverUnsigned")), facts.driverUnsigned, true},
        {Locale::tr(QStringLiteral("deep.driverTotal")), facts.driverTotal, true},
        {Locale::tr(QStringLiteral("deep.driverLatest")), facts.driverLatest, false, -1.0, true},
    });

    m_privacy->setRows({
        {Locale::tr(QStringLiteral("deep.telemetry")), facts.privacyTelemetry, false},
        {Locale::tr(QStringLiteral("deep.advertisingId")), facts.privacyAdvertisingId, false},
        {Locale::tr(QStringLiteral("deep.activityHistory")), facts.privacyActivityHistory, false},
        {Locale::tr(QStringLiteral("deep.location")), facts.privacyLocation, false},
        {Locale::tr(QStringLiteral("deep.inkTyping")), facts.privacyInkTyping, false},
        {Locale::tr(QStringLiteral("deep.privacyScore")), facts.privacyScore, false},
    });

    // The list-shaped blocks carry one row per volume or per disk, and a placeholder
    // while the stage that fills them is still running — an empty card reads as broken.
    QVector<InfoRow> encryption;
    for (const DeepInfo::Entry &entry : facts.encryption)
        encryption.append({entry.name, entry.detail, false, entry.meter, true});
    if (encryption.isEmpty())
        encryption.append({Locale::tr(QStringLiteral("ov.birim")), QStringLiteral("—"), false, -1.0});
    encryption.append({Locale::tr(QStringLiteral("deep.recoveryKey")), facts.recoveryKey, false, -1.0});
    encryption.append({Locale::tr(QStringLiteral("deep.tpmOwnership")), facts.tpmOwnership, false, -1.0});
    m_encryption->setRows(encryption);

    m_accounts->setRows({
        {Locale::tr(QStringLiteral("deep.localAccounts")), facts.accountsLocal, false},
        {Locale::tr(QStringLiteral("deep.guest")), facts.accountsGuest, false},
        {Locale::tr(QStringLiteral("deep.uacLevel")), facts.uacLevel, false},
        {Locale::tr(QStringLiteral("deep.passwordPolicy")), facts.passwordPolicy, false},
        {Locale::tr(QStringLiteral("deep.passwordAge")), facts.passwordAge, true},
    });

    m_virtualisation->setRows({
        {QStringLiteral("Hyper-V"), facts.hyperV, false},
        {Locale::tr(QStringLiteral("deep.vbs")), facts.vbs, false},
        {QStringLiteral("WSL"), facts.wsl, false},
        {Locale::tr(QStringLiteral("deep.sandbox")), facts.sandbox, false},
        {Locale::tr(QStringLiteral("deep.credentialGuard")), facts.credentialGuard, false},
    });

    // Stacked: a disk's name and its reliability counters are each a line's worth of
    // text, and side by side neither one survives.
    QVector<InfoRow> disks;
    for (const DeepInfo::Entry &entry : facts.disks)
        disks.append({entry.name, entry.detail, false, entry.meter, true});
    if (disks.isEmpty())
        disks.append({Locale::tr(QStringLiteral("deep.disk")), QStringLiteral("—"), false, -1.0});
    disks.append({Locale::tr(QStringLiteral("deep.trim")), facts.trim, false, -1.0});
    disks.append({Locale::tr(QStringLiteral("deep.partitionStyle")), facts.partitionStyle, true, -1.0});
    m_diskHealth->setRows(disks);

    m_performance->setRows({
        {QStringLiteral("WinSAT"), facts.winsat, true},
        {Locale::tr(QStringLiteral("deep.bootDuration")), facts.bootDuration, true},
        {Locale::tr(QStringLiteral("deep.pageFile")), facts.pageFileUsage, true},
        {Locale::tr(QStringLiteral("deep.commitCharge")), facts.commitCharge, true},
    });

    m_connection->setRows({
        {QStringLiteral("DHCP"), facts.dhcp, false},
        {Locale::tr(QStringLiteral("deep.proxy")), facts.proxy, true},
        {QStringLiteral("DNS-over-HTTPS"), facts.doh, false},
        {QStringLiteral("Wi-Fi"), facts.wifi, false},
        {Locale::tr(QStringLiteral("deep.activeConnections")), facts.activeConnections, true},
        {Locale::tr(QStringLiteral("deep.metered")), facts.metered, false},
    });

    m_sensors->setRows({
        {Locale::tr(QStringLiteral("deep.cpuTemp")), facts.cpuTemperature, true},
        {Locale::tr(QStringLiteral("deep.gpuTemp")), facts.gpuTemperature, true},
        {Locale::tr(QStringLiteral("deep.fan")), facts.fan, true},
        {Locale::tr(QStringLiteral("deep.gpuFan")), facts.gpuFan, true},
        {Locale::tr(QStringLiteral("deep.batteryHealth")), facts.batteryHealth, true},
        {Locale::tr(QStringLiteral("deep.batteryCycles")), facts.batteryCycles, true},
        {Locale::tr(QStringLiteral("deep.batteryChemistry")), facts.batteryChemistry, false},
    });

    m_graphics->setRows({
        {Locale::tr(QStringLiteral("deep.gpuUtil")), facts.gpuUtilisation, true},
        {Locale::tr(QStringLiteral("deep.gpuMemory")), facts.gpuMemory, true},
        {Locale::tr(QStringLiteral("deep.gpuPower")), facts.gpuPower, true},
        {Locale::tr(QStringLiteral("deep.gpuClock")), facts.gpuClock, true},
        {Locale::tr(QStringLiteral("deep.gpuPcie")), facts.gpuPcie, true},
    });

    m_protection->setRows({
        {Locale::tr(QStringLiteral("deep.antivirus")), facts.antivirus, false},
        {Locale::tr(QStringLiteral("deep.signatures")), facts.signatures, true},
        {Locale::tr(QStringLiteral("deep.lastScan")), facts.lastScan, true},
        {Locale::tr(QStringLiteral("deep.tamper")), facts.tamperProtection, false},
        {Locale::tr(QStringLiteral("deep.firewallProfiles")), facts.firewallProfiles, false},
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
