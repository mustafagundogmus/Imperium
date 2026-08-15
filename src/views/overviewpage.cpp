#include "overviewpage.h"
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
constexpr int SectionGapV = 18;
constexpr int SectionGapH = 28;

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

    m_tileCpu = new StatTile(QStringLiteral("İşlemci"), tiles);
    m_tileRam = new StatTile(QStringLiteral("Bellek"), tiles);
    m_tileDisk = new StatTile(QStringLiteral("Depolama"), tiles);
    m_tileNet = new StatTile(QStringLiteral("Ağ · indirme"), tiles);

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

    auto *chartHeader = new SectionHeader(QStringLiteral("Canlı izleme"), chartBlock);
    chartHeader->setCount(QStringLiteral("1 sn aralık"));
    chartLayout->addWidget(chartHeader);

    m_chart = new LiveChart(chartBlock);
    chartLayout->addWidget(m_chart);

    outer->addWidget(chartBlock);

    // --- info blocks --------------------------------------------------------
    auto *sections = new QWidget(this);
    auto *grid = new QGridLayout(sections);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(SectionGapH);
    grid->setVerticalSpacing(SectionGapV);

    m_system = new InfoSection(QStringLiteral("Sistem"), sections);
    m_user = new InfoSection(QStringLiteral("Kullanıcı"), sections);
    m_hardware = new InfoSection(QStringLiteral("Donanım"), sections);
    m_display = new InfoSection(QStringLiteral("Ekran"), sections);
    m_network = new InfoSection(QStringLiteral("Ağ"), sections);
    m_power = new InfoSection(QStringLiteral("Güç"), sections);
    m_security = new InfoSection(QStringLiteral("Güvenlik"), sections);
    m_storage = new InfoSection(QStringLiteral("Depolama"), sections);
    m_session = new InfoSection(QStringLiteral("Oturum"), sections);
    m_firmware = new InfoSection(QStringLiteral("Bellenim"), sections);
    m_processor = new InfoSection(QStringLiteral("İşlemci"), sections);
    m_memory = new InfoSection(QStringLiteral("Bellek"), sections);
    m_software = new InfoSection(QStringLiteral("Yazılım"), sections);
    m_locale = new InfoSection(QStringLiteral("Zaman & bölge"), sections);
    m_processes = new InfoSection(QStringLiteral("Süreçler"), sections);

    // Three columns rather than the handoff's two: there is a lot to show and the window
    // is wide enough that a third column costs nothing but gains a whole screen of
    // vertical space. Ordered so related blocks sit near each other.
    const QVector<InfoSection *> order{
        m_system,    m_hardware,  m_processor,
        m_user,      m_memory,    m_firmware,
        m_display,   m_storage,   m_network,
        m_security,  m_power,     m_software,
        m_session,   m_processes, m_locale,
    };
    for (int i = 0; i < order.size(); ++i)
        grid->addWidget(order.at(i), i / 3, i % 3, Qt::AlignTop);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);

    outer->addWidget(sections);
    outer->addStretch(1);

    connect(m_monitor, &SystemMonitor::sampled, this, &OverviewPage::onSampled);
    onSampled(m_monitor->latest());
    setFacts(m_facts);
}

void OverviewPage::onSampled(const Sample &sample)
{
    m_tileCpu->setValue(percent(sample.cpuPercent));
    m_tileCpu->setSub(m_monitor->physicalCores() > 0
                          ? QStringLiteral("%1 çekirdek · %2 iş parçacığı")
                                .arg(m_monitor->physicalCores())
                                .arg(m_monitor->logicalCores())
                          : QStringLiteral("%1 iş parçacığı").arg(m_monitor->logicalCores()));

    m_tileRam->setValue(percent(sample.ramPercent));
    m_tileRam->setSub(sample.ramTotal > 0
                          ? QStringLiteral("%1 / %2").arg(bytes(sample.ramUsed), bytesWhole(sample.ramTotal))
                          : QStringLiteral("—"));

    const qreal diskPercent = sample.diskTotal > 0
                                  ? 100.0 * qreal(sample.diskUsed) / qreal(sample.diskTotal)
                                  : 0.0;
    m_tileDisk->setValue(percent(diskPercent));
    m_tileDisk->setSub(sample.diskTotal > 0
                           ? QStringLiteral("%1 boş · %2")
                                 .arg(bytes(sample.diskTotal - sample.diskUsed), bytes(sample.diskTotal))
                           : QStringLiteral("—"));

    m_tileNet->setValue(rate(sample.downBytesPerSec));
    m_tileNet->setSub(QStringLiteral("gönderme: %1").arg(rate(sample.upBytesPerSec)));

    m_chart->setSeries(m_monitor->cpuHistory(), m_monitor->ramHistory(), SystemMonitor::HistorySize);

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
        {QStringLiteral("İşletim sistemi"), facts.osName, false},
        {QStringLiteral("Sürüm"), facts.version, true},
        {QStringLiteral("Yükleme tarihi"), facts.installDate, true},
        {QStringLiteral("Son güncelleme"), facts.lastUpdate, true},
        {QStringLiteral("Etkinleştirme"), facts.activation, false},
        {QStringLiteral("Güvenli Önyükleme"), facts.secureBoot, false},
        {QStringLiteral("TPM"), facts.tpm, true},
        {QStringLiteral("Sürüm dalı"), facts.buildBranch, true},
        {QStringLiteral("Edisyon kimliği"), facts.editionId, true},
    });

    m_user->setRows({
        {QStringLiteral("Kullanıcı adı"), facts.userName, true},
        {QStringLiteral("Hesap türü"), facts.accountType, false},
        {QStringLiteral("Bilgisayar adı"), facts.computerName, true},
        {QStringLiteral("Microsoft hesabı"), facts.microsoftAccount, false},
        {QStringLiteral("Etkin grup ilkesi"), facts.activePolicies, true},
        {QStringLiteral("Profil klasörü"), facts.profilePath, true},
    });

    m_hardware->setRows({
        {QStringLiteral("İşlemci"), facts.cpu, false},
        {QStringLiteral("Bellek"), facts.memory, false},
        {QStringLiteral("Ekran kartı"), facts.gpu, false},
        {QStringLiteral("Depolama"), facts.storage, false},
        {QStringLiteral("Anakart"), facts.motherboard, false},
        {QStringLiteral("BIOS"), facts.bios, true},
    });

    m_display->setRows({
        {QStringLiteral("Ekran sayısı"), facts.displayCount, false},
        {QStringLiteral("Çözünürlük"), facts.resolution, true},
        {QStringLiteral("Renk derinliği"), facts.colorDepth, true},
        {QStringLiteral("Ölçekleme"), facts.dpiScale, true},
        {QStringLiteral("Ekran sürücüsü"), facts.graphicsDriver, true},
        {QStringLiteral("Sanal masaüstü"), facts.virtualDesktop, true},
    });

    m_network->setRows({
        {QStringLiteral("Bağdaştırıcı"), facts.adapter, false},
        {QStringLiteral("IPv4 adresi"), facts.ipv4, true},
        {QStringLiteral("Bağlantı hızı"), facts.linkSpeed, true},
        {QStringLiteral("DNS sunucusu"), facts.dnsServer, true},
        {QStringLiteral("IPv6 adresi"), facts.ipv6, true},
        {QStringLiteral("Ağ geçidi"), facts.gateway, true},
        {QStringLiteral("Bağdaştırıcı"), facts.adapterCount, false},
        {QStringLiteral("Etki alanı"), facts.domain, false},
    });

    m_power->setRows({
        {QStringLiteral("Güç kaynağı"), facts.powerSource, false},
        {QStringLiteral("Pil"), facts.battery, false},
        {QStringLiteral("Güç planı"), facts.powerPlan, false},
    });

    m_security->setRows({
        {QStringLiteral("Microsoft Defender"), facts.defender, false},
        {QStringLiteral("Güvenlik duvarı"), facts.firewall, false},
        {QStringLiteral("SmartScreen"), facts.smartScreen, false},
        {QStringLiteral("Çekirdek yalıtımı"), facts.coreIsolation, false},
        {QStringLiteral("Sanallaştırma güvenliği"), facts.virtualization, false},
    });

    QVector<InfoRow> volumes;
    volumes.reserve(facts.volumes.size());
    for (const auto &volume : facts.volumes)
        volumes.append({QStringLiteral("Birim %1").arg(volume.first), volume.second, true});
    if (volumes.isEmpty())
        volumes.append({QStringLiteral("Birim"), QStringLiteral("—"), false});
    m_storage->setRows(volumes);

    m_firmware->setRows({
        {QStringLiteral("Üretici"), facts.manufacturer, false},
        {QStringLiteral("Model"), facts.model, false},
        {QStringLiteral("Anakart"), facts.motherboard, false},
        {QStringLiteral("BIOS üreticisi"), facts.biosVendor, false},
        {QStringLiteral("BIOS tarihi"), facts.biosDate, true},
        {QStringLiteral("SMBIOS"), facts.smbios, true},
        {QStringLiteral("Önyükleme modu"), facts.bootMode, false},
    });

    m_processor->setRows({
        {QStringLiteral("Model"), facts.cpu, false},
        {QStringLiteral("Üretici"), facts.cpuVendor, true},
        {QStringLiteral("Taban frekans"), facts.cpuBaseClock, true},
        {QStringLiteral("Mimari"), facts.cpuArchitecture, true},
        {QStringLiteral("Sanallaştırma"), facts.cpuVirtualization, false},
    });

    m_memory->setRows({
        {QStringLiteral("Takılı"), facts.memory, false},
        {QStringLiteral("Kullanımda"), facts.memoryInUse, true},
        {QStringLiteral("Boşta"), facts.memoryFree, true},
        {QStringLiteral("Sayfa dosyası"), facts.pageFile, true},
        {QStringLiteral("Yuvalar"), facts.memorySlots, true},
    });

    m_software->setRows({
        {QStringLiteral("Kurulu program"), facts.installedPrograms, true},
        {QStringLiteral("Başlangıç girişi"), facts.startupEntries, true},
        {QStringLiteral(".NET Framework"), facts.dotNet, true},
        {QStringLiteral("PowerShell"), facts.powerShell, true},
        {QStringLiteral("Varsayılan tarayıcı"), facts.defaultBrowser, false},
    });

    m_locale->setRows({
        {QStringLiteral("Saat dilimi"), facts.timeZone, false},
        {QStringLiteral("NTP sunucusu"), facts.ntpServer, true},
        {QStringLiteral("Dil ve bölge"), facts.locale, false},
        {QStringLiteral("Klavye düzeni"), facts.keyboardLayout, true},
    });

    m_processes->setRows({
        {QStringLiteral("Çalışan süreç"), facts.processCount, true},
        {QStringLiteral("İş parçacığı"), facts.threadCount, true},
        {QStringLiteral("Tanıtıcı"), facts.handleCount, true},
        {QStringLiteral("Boşta süre"), facts.idleTime, true},
    });

    m_session->setRows({
        {QStringLiteral("Çalışma süresi"), facts.uptime, true},
        {QStringLiteral("Son önyükleme"), facts.lastBoot, true},
        {QStringLiteral("Son oturum açma"), facts.lastLogon, true},
        {QStringLiteral("Bekleyen yeniden başlatma"), facts.pendingRestart, false},
        {QStringLiteral("Son geri yükleme noktası"), facts.lastRestorePoint, true},
    });
}
