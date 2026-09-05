#include "downloadprogresswidget.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"
#include "buttons.h"

#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString formatSizeInternal(qint64 bytes)
{
    if (bytes <= 0)
        return QStringLiteral("0 B");
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024LL * 1024LL) {
        const double kb = double(bytes) / 1024.0;
        return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
    }
    if (bytes >= 1024LL * 1024LL * 1024LL) {
        const double gb = double(bytes) / (1024.0 * 1024.0 * 1024.0);
        return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
    }
    const double mb = double(bytes) / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}

QString formatEtaInternal(int seconds)
{
    if (seconds <= 0)
        return QStringLiteral("--:--");
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    if (h > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

} // namespace

DownloadProgressWidget::DownloadProgressWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(8);

    // ─── 1. Satır: Başlık, Durum Rozeti, Parça Rozeti ve Genel Yüzde ───
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setFont(Theme::sans(13.5, Theme::Weight::SemiBold));
    headerLayout->addWidget(m_titleLabel);

    headerLayout->addSpacing(8);

    m_badgeLabel = new QLabel(this);
    m_badgeLabel->setFont(Theme::sans(10.5, Theme::Weight::Medium));
    m_badgeLabel->setStyleSheet(QStringLiteral("padding: 2px 8px; border-radius: 4px; background: rgba(120, 120, 120, 0.2);"));
    headerLayout->addWidget(m_badgeLabel);

    headerLayout->addSpacing(6);

    m_partsBadge = new QLabel(this);
    m_partsBadge->setFont(Theme::sans(10.5, Theme::Weight::SemiBold));
    m_partsBadge->setStyleSheet(QStringLiteral("padding: 2px 8px; border-radius: 4px; background: rgba(59, 130, 246, 0.15); color: #3b82f6;"));
    m_partsBadge->hide();
    headerLayout->addWidget(m_partsBadge);

    headerLayout->addStretch(1);

    m_pctLabel = new QLabel(QStringLiteral("%0.0"), this);
    m_pctLabel->setFont(Theme::sans(18.0, Theme::Weight::SemiBold));
    m_pctLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_pctLabel->setMinimumWidth(75);
    headerLayout->addWidget(m_pctLabel);
    layout->addLayout(headerLayout);

    // ─── 2. Satır: GENEL / TOPLAM İLERLEME ÇUBUĞU ───
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    layout->addWidget(m_progressBar);

    // ─── 3. Satır: AKTİF DOSYA BÖLÜMÜ (Çift Bar - Çoklu pakette görünür) ───
    m_fileContainer = new QWidget(this);
    auto *fileLayout = new QVBoxLayout(m_fileContainer);
    fileLayout->setContentsMargins(0, 4, 0, 2);
    fileLayout->setSpacing(4);

    auto *fileHeaderLayout = new QHBoxLayout();
    fileHeaderLayout->setContentsMargins(0, 0, 0, 0);

    m_activeFileLabel = new QLabel(this);
    m_activeFileLabel->setFont(Theme::sans(11.0, Theme::Weight::Medium));
    fileHeaderLayout->addWidget(m_activeFileLabel, 1);

    m_filePctLabel = new QLabel(QStringLiteral("%0.0"), this);
    m_filePctLabel->setFont(Theme::sans(11.0, Theme::Weight::SemiBold));
    m_filePctLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_filePctLabel->setMinimumWidth(55);
    fileHeaderLayout->addWidget(m_filePctLabel);
    fileLayout->addLayout(fileHeaderLayout);

    m_fileProgressBar = new QProgressBar(this);
    m_fileProgressBar->setFixedHeight(6);
    m_fileProgressBar->setRange(0, 1000);
    m_fileProgressBar->setValue(0);
    m_fileProgressBar->setTextVisible(false);
    fileLayout->addWidget(m_fileProgressBar);

    m_fileContainer->hide();
    layout->addWidget(m_fileContainer);

    // ─── 4. Satır: Metrikler (İnen / Kalan / Hız / ETA) ───
    auto *metricsLayout = new QHBoxLayout();
    metricsLayout->setContentsMargins(0, 0, 0, 0);
    metricsLayout->setSpacing(16);

    const QFont metricFont = Theme::Font::tweakDesc();

    m_downloadedLabel = new QLabel(this);
    m_downloadedLabel->setFont(metricFont);
    m_downloadedLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    metricsLayout->addWidget(m_downloadedLabel);

    m_remainingLabel = new QLabel(this);
    m_remainingLabel->setFont(metricFont);
    m_remainingLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    metricsLayout->addWidget(m_remainingLabel);

    m_speedLabel = new QLabel(this);
    m_speedLabel->setFont(metricFont);
    m_speedLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    metricsLayout->addWidget(m_speedLabel);

    m_etaLabel = new QLabel(this);
    m_etaLabel->setFont(metricFont);
    m_etaLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    metricsLayout->addWidget(m_etaLabel);

    metricsLayout->addStretch(1);
    layout->addLayout(metricsLayout);

    // ─── 4b. Satır: SHA-256 Hash ve Panoya Kopyalama ───
    m_hashContainer = new QWidget(this);
    auto *hashLayout = new QHBoxLayout(m_hashContainer);
    hashLayout->setContentsMargins(0, 2, 0, 2);
    hashLayout->setSpacing(8);

    auto *hashBadge = new QLabel(QStringLiteral("SHA-256"), m_hashContainer);
    hashBadge->setFont(Theme::sans(9.5, Theme::Weight::SemiBold));
    hashBadge->setStyleSheet(QStringLiteral("padding: 2px 6px; border-radius: 4px; background: rgba(59, 130, 246, 0.15); color: #3b82f6;"));
    hashLayout->addWidget(hashBadge);

    m_hashLabel = new QLabel(m_hashContainer);
    m_hashLabel->setFont(Theme::sans(10.0, Theme::Weight::Medium));
    m_hashLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hashLayout->addWidget(m_hashLabel, 1);

    m_copyHashBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.progress.copy_hash")), m_hashContainer);
    m_copyHashBtn->setFixedHeight(24);
    connect(m_copyHashBtn, &PillButton::clicked, this, [this] {
        if (!m_currentSha256.isEmpty()) {
            QGuiApplication::clipboard()->setText(m_currentSha256);
            m_copyHashBtn->setText(Locale::tr(QStringLiteral("office.progress.hash_copied")));
            QTimer::singleShot(2000, this, [this] {
                if (m_copyHashBtn) {
                    m_copyHashBtn->setText(Locale::tr(QStringLiteral("office.progress.copy_hash")));
                }
            });
        }
    });
    hashLayout->addWidget(m_copyHashBtn);

    m_hashContainer->hide();
    layout->addWidget(m_hashContainer);

    // ─── 5. Satır: Kontrol Düğmeleri ───
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(8);

    m_pauseResumeBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.progress.pause")), this);
    connect(m_pauseResumeBtn, &PillButton::clicked, this, [this] {
        if (!m_downloader) return;
        if (m_downloader->state() == OfficeDownloader::State::Downloading) {
            m_downloader->pause();
        } else if (m_downloader->state() == OfficeDownloader::State::Paused) {
            m_downloader->resume();
        }
    });
    btnLayout->addWidget(m_pauseResumeBtn);

    m_cancelBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.progress.cancel")), this);
    connect(m_cancelBtn, &PillButton::clicked, this, [this] {
        if (m_downloader) m_downloader->cancel();
    });
    btnLayout->addWidget(m_cancelBtn);

    m_toolCombo = new QComboBox(this);
    m_toolCombo->setFixedHeight(28);
    m_toolCombo->setFont(Theme::sans(10.5, Theme::Weight::Medium));
    m_toolCombo->addItem(QStringLiteral("Aria2"), QStringLiteral("aria2"));
    m_toolCombo->addItem(QStringLiteral("cURL"), QStringLiteral("curl"));
    m_toolCombo->addItem(QStringLiteral("Wget"), QStringLiteral("wget"));
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.internal")), QStringLiteral("internal"));
    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (!m_downloader || idx < 0) return;
        const QString tool = m_toolCombo->itemData(idx).toString();
        if (m_downloader->currentTool() != tool) {
            m_downloader->switchTool(tool);
        }
    });
    btnLayout->addWidget(m_toolCombo);

    m_openFolderBtn = new PillButton(PillButton::Accent, Locale::tr(QStringLiteral("office.progress.open_folder")), this);
    m_openFolderBtn->hide();
    connect(m_openFolderBtn, &PillButton::clicked, this, [this] {
        Q_EMIT openFolderRequested(m_filePath);
    });
    btnLayout->addWidget(m_openFolderBtn);

    m_mountBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.progress.mount")), this);
    m_mountBtn->hide();
    connect(m_mountBtn, &PillButton::clicked, this, [this] {
        Q_EMIT mountRequested(m_filePath);
    });
    btnLayout->addWidget(m_mountBtn);

    m_closeBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.dialog.cancel")), this);
    m_closeBtn->hide();
    connect(m_closeBtn, &PillButton::clicked, this, [this] {
        hide();
        Q_EMIT closed();
    });
    btnLayout->addWidget(m_closeBtn);

    layout->addLayout(btnLayout);

    // ─── 6. Satır: Paket İndirme Dosya Listesi ───
    m_packageFilesContainer = new QWidget(this);
    auto *pkgLayout = new QVBoxLayout(m_packageFilesContainer);
    pkgLayout->setContentsMargins(0, 4, 0, 0);
    pkgLayout->setSpacing(4);

    m_packageFilesHeader = new QLabel(m_packageFilesContainer);
    m_packageFilesHeader->setFont(Theme::sans(10.0, Theme::Weight::SemiBold));
    m_packageFilesHeader->setStyleSheet(QStringLiteral("color: #888888;"));
    pkgLayout->addWidget(m_packageFilesHeader);

    m_fileListWidget = new QWidget(m_packageFilesContainer);
    m_fileListLayout = new QVBoxLayout(m_fileListWidget);
    m_fileListLayout->setContentsMargins(8, 6, 8, 6);
    m_fileListLayout->setSpacing(2);

    pkgLayout->addWidget(m_fileListWidget);

    m_packageFilesContainer->hide();
    layout->addWidget(m_packageFilesContainer);

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, [this] {
        retranslate();
        update();
    });
    connect(Theme::notifier(), &Theme::Notifier::accentChanged,    this, [this] { retranslate(); });
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged,  this, [this] { retranslate(); });
    connect(Theme::notifier(), &Theme::Notifier::compactChanged,   this, [this] { retranslate(); });
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &DownloadProgressWidget::retranslate);

    retranslate();
}

void DownloadProgressWidget::setDownloader(OfficeDownloader *downloader)
{
    if (m_downloader == downloader)
        return;

    m_downloader = downloader;
    if (m_downloader) {
        connect(m_downloader, &OfficeDownloader::progress, this, &DownloadProgressWidget::updateProgress);
        connect(m_downloader, &OfficeDownloader::fileProgress, this, &DownloadProgressWidget::updateFileProgress);
        connect(m_downloader, &OfficeDownloader::stateChanged, this, &DownloadProgressWidget::updateState);
        connect(m_downloader, &OfficeDownloader::hashReady, this, [this](const QString &path, const QString &hash) {
            Q_UNUSED(path);
            m_currentSha256 = hash;
            if (!hash.isEmpty()) {
                m_hashLabel->setText(QStringLiteral("%1: %2")
                                        .arg(Locale::tr(QStringLiteral("office.progress.hash_label")), hash));
                m_hashContainer->show();
            }
        });
        connect(m_downloader, &OfficeDownloader::finished, this, [this](bool ok, const QString &path, const QString &err) {
            m_filePath = path;
            if (!ok && !err.isEmpty()) {
                m_badgeLabel->setText(QStringLiteral("Hata: %1").arg(err));
            }
        });
        connect(m_downloader, &OfficeDownloader::packageFilesInitialized, this, &DownloadProgressWidget::initPackageFiles);
        connect(m_downloader, &OfficeDownloader::packageFileStatusChanged, this, &DownloadProgressWidget::updatePackageFileStatus);
    }
}

void DownloadProgressWidget::reset()
{
    m_lastReceived = 0;
    m_lastTotal = 0;
    m_lastSpeed = 0.0;
    m_lastEta = 0;
    m_completedFiles = 0;
    m_totalFiles = 1;
    m_activeFileName.clear();
    m_lastFileReceived = 0;
    m_lastFileTotal = 0;
    m_lastFilePct = 0.0;
    m_currentSha256.clear();

    for (auto &row : m_fileRows) {
        if (row.rowWidget) {
            row.rowWidget->deleteLater();
        }
    }
    m_fileRows.clear();
    if (m_packageFilesContainer) {
        m_packageFilesContainer->hide();
    }

    m_partsBadge->hide();
    m_fileContainer->hide();
    m_hashContainer->hide();
    if (m_hashLabel) m_hashLabel->clear();
    m_lastState = OfficeDownloader::State::Idle;
    m_progressBar->setValue(0);
    m_fileProgressBar->setValue(0);
    m_pctLabel->setText(QStringLiteral("%0.0"));
    m_filePctLabel->setText(QStringLiteral("%0.0"));
    m_downloadedLabel->setText(QString());
    m_remainingLabel->setText(QString());
    m_speedLabel->setText(QString());
    m_etaLabel->setText(QString());
    m_pauseResumeBtn->show();
    m_cancelBtn->show();
    if (m_toolCombo) {
        m_toolCombo->show();
        m_toolCombo->setEnabled(true);
        m_toolCombo->setToolTip(QString());
    }
    m_openFolderBtn->hide();
    m_mountBtn->hide();
    m_closeBtn->hide();
}

void DownloadProgressWidget::updateFileProgress(int completedFiles, int totalFiles, const QString &currentFileName,
                                              qint64 fileReceived, qint64 fileTotal, double filePct)
{
    m_completedFiles = completedFiles;
    m_totalFiles = totalFiles;
    m_activeFileName = currentFileName;
    m_lastFileReceived = fileReceived;
    m_lastFileTotal = fileTotal;
    m_lastFilePct = filePct;

    if (totalFiles > 1) {
        m_partsBadge->setText(Locale::tr(QStringLiteral("office.progress.parts")).arg(completedFiles).arg(totalFiles));
        m_partsBadge->show();
        m_fileContainer->show();

        QString fileText;
        if (fileTotal > 0) {
            fileText = QStringLiteral("📄 %1 (%2 / %3)")
                           .arg(currentFileName, formatSizeInternal(fileReceived), formatSizeInternal(fileTotal));
        } else {
            fileText = QStringLiteral("📄 %1").arg(currentFileName);
        }
        m_activeFileLabel->setText(fileText);

        const double pctClamped = qBound(0.0, filePct, 100.0);
        m_fileProgressBar->setValue(qRound(pctClamped * 10.0));
        m_filePctLabel->setText(QStringLiteral("%%1").arg(pctClamped, 0, 'f', 1));
    } else {
        m_partsBadge->hide();
        m_fileContainer->hide();
    }
}

void DownloadProgressWidget::updateProgress(qint64 received, qint64 total, double speedMBps, int etaSeconds)
{
    m_lastReceived = received;
    m_lastTotal = total;
    m_lastSpeed = speedMBps;
    m_lastEta = etaSeconds;

    double pct = (total > 0) ? (double(received) / double(total) * 100.0) : 0.0;
    pct = qBound(0.0, pct, 100.0);

    m_progressBar->setValue(qRound(pct * 10.0));
    m_pctLabel->setText(QStringLiteral("%%1").arg(pct, 0, 'f', 1));

    const QString dlStr = Locale::tr(QStringLiteral("office.progress.downloaded"))
                              .arg(formatSizeInternal(received))
                              .arg(formatSizeInternal(total));
    m_downloadedLabel->setText(dlStr);

    const qint64 remaining = (total > received) ? (total - received) : 0;
    const QString remStr = Locale::tr(QStringLiteral("office.progress.remaining"))
                              .arg(formatSizeInternal(remaining));
    m_remainingLabel->setText(remStr);

    const QString speedStr = Locale::tr(QStringLiteral("office.progress.speed"))
                                .arg(speedMBps, 0, 'f', 2);
    m_speedLabel->setText(speedStr);

    const QString etaStr = Locale::tr(QStringLiteral("office.progress.eta"))
                              .arg(formatEtaInternal(etaSeconds));
    m_etaLabel->setText(etaStr);
}

void DownloadProgressWidget::updateState(OfficeDownloader::State state)
{
    m_lastState = state;
    if (m_downloader) {
        m_titleLabel->setText(m_downloader->currentFileName());
        const QString tool = m_downloader->currentTool();
        for (int i = 0; i < m_toolCombo->count(); ++i) {
            if (m_toolCombo->itemData(i).toString() == tool) {
                if (m_toolCombo->currentIndex() != i) {
                    const QSignalBlocker blocker(m_toolCombo);
                    m_toolCombo->setCurrentIndex(i);
                }
                break;
            }
        }
    }

    switch (state) {
    case OfficeDownloader::State::Downloading:
        m_badgeLabel->setText(Locale::tr(QStringLiteral("office.progress.downloading")));
        m_badgeLabel->setStyleSheet(QStringLiteral("padding: 2px 8px; border-radius: 4px; background: rgba(52, 199, 89, 0.2); color: #34c759;"));
        m_pauseResumeBtn->setText(Locale::tr(QStringLiteral("office.progress.pause")));
        m_pauseResumeBtn->show();
        m_cancelBtn->show();
        m_toolCombo->show();
        m_toolCombo->setEnabled(false);
        m_toolCombo->setToolTip(Locale::tr(QStringLiteral("office.progress.tool_paused_only")));
        m_openFolderBtn->hide();
        m_mountBtn->hide();
        m_closeBtn->hide();
        m_hashContainer->hide();
        break;
    case OfficeDownloader::State::Paused:
        m_badgeLabel->setText(Locale::tr(QStringLiteral("office.progress.paused")));
        m_badgeLabel->setStyleSheet(QStringLiteral("padding: 2px 8px; border-radius: 4px; background: rgba(255, 149, 0, 0.2); color: #ff9500;"));
        m_pauseResumeBtn->setText(Locale::tr(QStringLiteral("office.progress.resume")));
        m_pauseResumeBtn->show();
        m_cancelBtn->show();
        m_toolCombo->show();
        m_toolCombo->setEnabled(true);
        m_toolCombo->setToolTip(QString());
        m_openFolderBtn->hide();
        m_mountBtn->hide();
        m_closeBtn->hide();
        m_hashContainer->hide();
        break;
    case OfficeDownloader::State::Verifying:
        m_badgeLabel->setText(Locale::tr(QStringLiteral("office.progress.verifying")));
        m_badgeLabel->setStyleSheet(QStringLiteral("padding: 2px 8px; border-radius: 4px; background: rgba(168, 85, 247, 0.2); color: #a855f7;"));
        m_pauseResumeBtn->hide();
        m_cancelBtn->hide();
        m_toolCombo->hide();
        m_openFolderBtn->hide();
        m_mountBtn->hide();
        m_closeBtn->hide();
        m_hashContainer->hide();
        break;
    case OfficeDownloader::State::Completed:
        m_badgeLabel->setText(Locale::tr(QStringLiteral("office.progress.completed")));
        m_badgeLabel->setStyleSheet(QStringLiteral("padding: 2px 8px; border-radius: 4px; background: rgba(52, 199, 89, 0.25); color: #34c759;"));
        m_pauseResumeBtn->hide();
        m_cancelBtn->hide();
        m_toolCombo->hide();
        m_openFolderBtn->show();
        m_mountBtn->show();
        m_closeBtn->show();

        if (m_downloader && !m_downloader->lastSha256().isEmpty()) {
            m_currentSha256 = m_downloader->lastSha256();
            m_hashLabel->setText(QStringLiteral("%1: %2")
                                    .arg(Locale::tr(QStringLiteral("office.progress.hash_label")), m_currentSha256));
            m_hashContainer->show();
        }

        if (m_downloader && m_downloader->isPackageMode()) {
            m_mountBtn->setText(Locale::tr(QStringLiteral("office.progress.start_setup")));
            m_partsBadge->setText(Locale::tr(QStringLiteral("office.progress.parts")).arg(m_totalFiles).arg(m_totalFiles));
            m_partsBadge->show();
            m_fileContainer->hide();
        } else {
            m_mountBtn->setText(Locale::tr(QStringLiteral("office.progress.mount")));
        }
        break;
    case OfficeDownloader::State::Failed:
        m_badgeLabel->setText(QStringLiteral("Hata Oluştu"));
        m_badgeLabel->setStyleSheet(QStringLiteral("padding: 2px 8px; border-radius: 4px; background: rgba(255, 59, 48, 0.2); color: #ff3b30;"));
        m_pauseResumeBtn->hide();
        m_cancelBtn->hide();
        m_toolCombo->hide();
        m_closeBtn->show();
        m_hashContainer->hide();
        break;
    case OfficeDownloader::State::Cancelled:
        m_toolCombo->hide();
        m_hashContainer->hide();
        hide();
        break;
    case OfficeDownloader::State::Idle:
        m_hashContainer->hide();
        break;
    }

    if (m_downloader && m_toolCombo) {
        m_toolCombo->blockSignals(true);
        const QString curTool = m_downloader->currentTool();
        for (int i = 0; i < m_toolCombo->count(); ++i) {
            if (m_toolCombo->itemData(i).toString() == curTool) {
                m_toolCombo->setCurrentIndex(i);
                break;
            }
        }
        m_toolCombo->blockSignals(false);
    }
}

void DownloadProgressWidget::updateMetricWidths()
{
    const QFontMetrics fm(m_downloadedLabel ? m_downloadedLabel->font() : Theme::Font::tweakDesc());

    // Current language max sample strings to reserve dedicated column slots
    const QString sampleDl = Locale::tr(QStringLiteral("office.progress.downloaded"))
                                 .arg(QStringLiteral("999.99 GB"), QStringLiteral("999.99 GB"));
    const QString sampleRem = Locale::tr(QStringLiteral("office.progress.remaining"))
                                  .arg(QStringLiteral("999.99 GB"));
    const QString sampleSpeed = Locale::tr(QStringLiteral("office.progress.speed"))
                                    .arg(QStringLiteral("999.99"));
    const QString sampleEta = Locale::tr(QStringLiteral("office.progress.eta"))
                                  .arg(QStringLiteral("99:59:59"));

    const int wDl = qMax(190, fm.horizontalAdvance(sampleDl) + 8);
    const int wRem = qMax(120, fm.horizontalAdvance(sampleRem) + 8);
    const int wSpeed = qMax(110, fm.horizontalAdvance(sampleSpeed) + 8);
    const int wEta = qMax(125, fm.horizontalAdvance(sampleEta) + 8);

    if (m_downloadedLabel) {
        m_downloadedLabel->setFixedWidth(wDl);
        m_downloadedLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (m_remainingLabel) {
        m_remainingLabel->setFixedWidth(wRem);
        m_remainingLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (m_speedLabel) {
        m_speedLabel->setFixedWidth(wSpeed);
        m_speedLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (m_etaLabel) {
        m_etaLabel->setFixedWidth(wEta);
        m_etaLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
}

void DownloadProgressWidget::retranslate()
{
    using namespace Theme;

    updateMetricWidths();

    const QColor accent = Theme::accent();
    m_pctLabel->setStyleSheet(QStringLiteral("color: %1;").arg(accent.name(QColor::HexRgb)));
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Color::TextPrimary().name(QColor::HexRgb)));
    m_filePctLabel->setStyleSheet(QStringLiteral("color: %1;").arg(accent.name(QColor::HexRgb)));
    m_activeFileLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: 500;").arg(Color::TextSecondary().name(QColor::HexRgb)));

    // Ana çubuk stili (Genel)
    const QString mainBarStyle = QStringLiteral(
        "QProgressBar { background: %1; border: none; border-radius: 4px; }"
        "QProgressBar::chunk { background: %2; border-radius: 4px; }")
        .arg(Color::BorderControl().name(QColor::HexRgb), accent.name(QColor::HexRgb));
    m_progressBar->setStyleSheet(mainBarStyle);

    // Alt çubuk stili (Aktif Dosya) — vurgu rengi %80 opaklıkta
    QColor accentDim = accent;
    accentDim.setAlphaF(0.80);
    const QString fileBarStyle = QStringLiteral(
        "QProgressBar { background: %1; border: none; border-radius: 3px; }"
        "QProgressBar::chunk { background: %2; border-radius: 3px; }")
        .arg(Color::BorderControl().name(QColor::HexRgb),
             accentDim.name(QColor::HexArgb));
    m_fileProgressBar->setStyleSheet(fileBarStyle);

    const QString textSec = Color::TextSecondary().name(QColor::HexRgb);
    m_downloadedLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSec));
    m_remainingLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSec));
    m_speedLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSec));
    m_etaLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSec));

    if (m_toolCombo) {
        const QString comboStyle = QStringLiteral(
            "QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 2px 8px; }"
            "QComboBox:disabled { background: rgba(120, 120, 120, 0.1); color: rgba(120, 120, 120, 0.5); border-color: rgba(120, 120, 120, 0.2); }"
            "QComboBox:hover:!disabled { border-color: %4; }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: %1; color: %2; selection-background-color: %4; }")
            .arg(Color::Surface().name(QColor::HexRgb),
                 Color::TextPrimary().name(QColor::HexRgb),
                 Color::BorderControl().name(QColor::HexRgb),
                 accent.name(QColor::HexRgb));
        m_toolCombo->setStyleSheet(comboStyle);
    }

    m_openFolderBtn->setText(Locale::tr(QStringLiteral("office.progress.open_folder")));
    if (m_downloader && m_downloader->isPackageMode()) {
        m_mountBtn->setText(Locale::tr(QStringLiteral("office.progress.start_setup")));
    } else {
        m_mountBtn->setText(Locale::tr(QStringLiteral("office.progress.mount")));
    }
    m_cancelBtn->setText(Locale::tr(QStringLiteral("office.progress.cancel")));
    m_closeBtn->setText(Locale::tr(QStringLiteral("office.dialog.cancel")));

    if (m_copyHashBtn) {
        m_copyHashBtn->setText(Locale::tr(QStringLiteral("office.progress.copy_hash")));
    }
    if (m_hashLabel) {
        m_hashLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSec));
        if (!m_currentSha256.isEmpty()) {
            m_hashLabel->setText(QStringLiteral("%1: %2")
                                    .arg(Locale::tr(QStringLiteral("office.progress.hash_label")), m_currentSha256));
        }
    }

    if (m_totalFiles > 1) {
        m_partsBadge->setText(Locale::tr(QStringLiteral("office.progress.parts")).arg(m_completedFiles).arg(m_totalFiles));
    }

    // ── Font boyutlarını güncelle (fontScale veya typeface değiştiğinde) ──
    m_titleLabel->setFont(Theme::sans(13.5, Theme::Weight::SemiBold));
    m_badgeLabel->setFont(Theme::sans(10.5, Theme::Weight::Medium));
    m_partsBadge->setFont(Theme::sans(10.5, Theme::Weight::SemiBold));
    m_pctLabel->setFont(Theme::sans(18.0, Theme::Weight::SemiBold));
    m_activeFileLabel->setFont(Theme::sans(11.0, Theme::Weight::Medium));
    m_filePctLabel->setFont(Theme::sans(11.0, Theme::Weight::SemiBold));

    const QFont metricFont = Theme::Font::tweakDesc();
    m_downloadedLabel->setFont(metricFont);
    m_remainingLabel->setFont(metricFont);
    m_speedLabel->setFont(metricFont);
    m_etaLabel->setFont(metricFont);

    if (m_hashLabel)
        m_hashLabel->setFont(Theme::sans(10.0, Theme::Weight::Medium));
    if (m_packageFilesHeader)
        m_packageFilesHeader->setFont(Theme::sans(10.0, Theme::Weight::SemiBold));

    // ── Compact mod: ana layout boşlukları ──
    const bool cmp = Theme::compact();
    if (auto *lay = qobject_cast<QVBoxLayout *>(layout()))
        lay->setSpacing(cmp ? 4 : 8);

    if (m_packageFilesHeader) {
        m_packageFilesHeader->setText(QStringLiteral("📦 ") + Locale::tr(QStringLiteral("office.progress.package_contents")));
        m_packageFilesHeader->setStyleSheet(QStringLiteral("color: %1;").arg(Color::TextSecondary().name(QColor::HexRgb)));
    }

    // Paket dosya listesi konteyner ve satır renkleri / fontları / compact padding
    if (m_fileListWidget) {
        const bool light = Theme::isLightFamily(Theme::appearance());
        const QString listBg   = light
            ? QStringLiteral("background: rgba(0,0,0,0.05); border-radius: 6px; border: 1px solid rgba(0,0,0,0.10);")
            : QStringLiteral("background: rgba(0,0,0,0.22); border-radius: 6px; border: 1px solid rgba(255,255,255,0.08);");
        m_fileListWidget->setStyleSheet(listBg);

        if (m_fileListLayout)
            m_fileListLayout->setSpacing(cmp ? 1 : 2);

        const QString rowBgNorm = light
            ? QStringLiteral("QWidget { background: rgba(0,0,0,0.03); border-radius: 4px; } QWidget:hover { background: rgba(0,0,0,0.07); }")
            : QStringLiteral("QWidget { background: rgba(255,255,255,0.02); border-radius: 4px; } QWidget:hover { background: rgba(255,255,255,0.05); }");
        const QString textPri  = Color::TextPrimary().name(QColor::HexRgb);
        const QString textMut  = Color::TextSecondary().name(QColor::HexRgb);
        const int rowV = cmp ? 1 : 3;   // compact: 1px, normal: 3px dikey dolgu

        for (auto &row : m_fileRows) {
            if (row.rowWidget) {
                row.rowWidget->setStyleSheet(rowBgNorm);
                // Compact mod satır dolgusu
                if (auto *rl = qobject_cast<QHBoxLayout *>(row.rowWidget->layout()))
                    rl->setContentsMargins(6, rowV, 6, rowV);
            }
            // Ad / boyut font güncelle
            if (row.nameLabel) {
                row.nameLabel->setFont(Theme::sans(10.0, Theme::Weight::Medium));
                row.nameLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textPri));
            }
            if (row.sizeLabel) {
                row.sizeLabel->setFont(Theme::sans(9.5));
                row.sizeLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textMut));
            }
            // Durum rengi / font da tema / fontScale ile yenilenir
            updatePackageFileStatus(static_cast<int>(&row - m_fileRows.data()),
                                    QString(), row.lastStatus, 0, QString());
        }
    }

    updateState(m_lastState);
    if (m_lastTotal > 0) {
        updateProgress(m_lastReceived, m_lastTotal, m_lastSpeed, m_lastEta);
    }
}

void DownloadProgressWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal rad = Theme::Metric::ControlRadius + 3.0;

    p.setPen(Theme::Color::BorderControl());
    p.setBrush(Theme::Color::Surface());
    p.drawRoundedRect(r, rad, rad);
}

QString DownloadProgressWidget::formatSize(qint64 bytes)
{
    return formatSizeInternal(bytes);
}

QString DownloadProgressWidget::formatEta(int seconds)
{
    return formatEtaInternal(seconds);
}

void DownloadProgressWidget::initPackageFiles(const QStringList &fileNames, const QList<qint64> &fileSizes)
{
    // Eski satırları temizle
    for (auto &row : m_fileRows) {
        if (row.rowWidget) {
            row.rowWidget->deleteLater();
        }
    }
    m_fileRows.clear();

    if (fileNames.isEmpty()) {
        m_packageFilesContainer->hide();
        return;
    }

    m_packageFilesContainer->show();

    // Tema bazlı renkler
    const bool light = Theme::isLightFamily(Theme::appearance());
    const QString rowBg = light
        ? QStringLiteral("QWidget { background: rgba(0,0,0,0.03); border-radius: 4px; } QWidget:hover { background: rgba(0,0,0,0.07); }")
        : QStringLiteral("QWidget { background: rgba(255,255,255,0.02); border-radius: 4px; } QWidget:hover { background: rgba(255,255,255,0.05); }");
    const QString textPri = Theme::Color::TextPrimary().name(QColor::HexRgb);
    const QString textMut = Theme::Color::TextSecondary().name(QColor::HexRgb);

    // Konteyner arka planını da güncelle
    if (m_fileListWidget) {
        const QString listBg = light
            ? QStringLiteral("background: rgba(0,0,0,0.05); border-radius: 6px; border: 1px solid rgba(0,0,0,0.10);")
            : QStringLiteral("background: rgba(0,0,0,0.22); border-radius: 6px; border: 1px solid rgba(255,255,255,0.08);");
        m_fileListWidget->setStyleSheet(listBg);
    }

    for (int i = 0; i < fileNames.size(); ++i) {
        const QString &fn = fileNames[i];
        const qint64 sz = (i < fileSizes.size()) ? fileSizes[i] : 0;

        auto *rowWidget = new QWidget(m_packageFilesContainer);
        rowWidget->setStyleSheet(rowBg);

        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(6, 3, 6, 3);
        rowLayout->setSpacing(8);

        auto *iconLabel = new QLabel(QStringLiteral("⏳"), rowWidget);
        iconLabel->setFont(Theme::sans(10.0));
        iconLabel->setFixedSize(22, 20);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textMut));
        rowLayout->addWidget(iconLabel);

        auto *nameLabel = new QLabel(fn, rowWidget);
        nameLabel->setFont(Theme::sans(10.0, Theme::Weight::Medium));
        nameLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textPri));
        rowLayout->addWidget(nameLabel);

        rowLayout->addStretch(1);

        auto *sizeLabel = new QLabel(formatSize(sz), rowWidget);
        sizeLabel->setFont(Theme::sans(9.5));
        sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sizeLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textMut));
        sizeLabel->setMinimumWidth(75);
        rowLayout->addWidget(sizeLabel);

        auto *statusLabel = new QLabel(Locale::tr(QStringLiteral("office.progress.file_pending")), rowWidget);
        statusLabel->setFont(Theme::sans(9.5));
        statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textMut));
        statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rowLayout->addWidget(statusLabel);

        m_fileListLayout->addWidget(rowWidget);

        FileRowWidgets w;
        w.rowWidget = rowWidget;
        w.iconLabel = iconLabel;
        w.nameLabel = nameLabel;
        w.sizeLabel = sizeLabel;
        w.statusLabel = statusLabel;
        m_fileRows.append(w);
    }
}

void DownloadProgressWidget::updatePackageFileStatus(int index, const QString &fileName, int status, qint64 size, const QString &sha256)
{
    Q_UNUSED(fileName);
    if (index < 0 || index >= m_fileRows.size())
        return;

    auto &row = m_fileRows[index];
    if (size > 0 && row.sizeLabel) {
        row.sizeLabel->setText(formatSize(size));
    }

    // Durumu kaydet (tema değişince retranslate() buradan yeniden çizer)
    row.lastStatus = status;
    if (!sha256.isEmpty())
        row.lastSha256 = sha256;

    if (!row.iconLabel || !row.statusLabel)
        return;

    // ── Tema bazlı renk türetme ──
    const bool light       = Theme::isLightFamily(Theme::appearance());
    const QColor accent    = Theme::accentInk();          // vurgu rengi (kullanıcı seçimi)
    const QString accentHex = accent.name(QColor::HexRgb);
    const QString mutHex    = Theme::Color::TextSecondary().name(QColor::HexRgb);

    // Açık temada sabit semantik renkler biraz koyulaştırılır ki okunabilir olsun
    const QString greenHex  = light ? QStringLiteral("#16a34a") : QStringLiteral("#22c55e");
    const QString redHex    = light ? QStringLiteral("#dc2626") : QStringLiteral("#ef4444");
    const QString purpleHex = light ? QStringLiteral("#7c3aed") : QStringLiteral("#a855f7");

    const auto fileStatus = static_cast<OfficeDownloader::FileStatus>(status);
    switch (fileStatus) {
    case OfficeDownloader::FileStatus::Pending:
        row.iconLabel->setText(QStringLiteral("⏳"));
        row.iconLabel->setFont(Theme::sans(10.0));
        row.iconLabel->setStyleSheet(QStringLiteral("color: %1;").arg(mutHex));
        row.statusLabel->setFont(Theme::sans(9.5));
        row.statusLabel->setText(Locale::tr(QStringLiteral("office.progress.file_pending")));
        row.statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(mutHex));
        break;

    case OfficeDownloader::FileStatus::Downloading:
        row.iconLabel->setText(QStringLiteral("⬇"));
        row.iconLabel->setFont(Theme::sans(11.0, Theme::Weight::SemiBold));
        row.iconLabel->setStyleSheet(QStringLiteral("color: %1;").arg(accentHex));
        row.statusLabel->setFont(Theme::sans(9.5, Theme::Weight::Medium));
        row.statusLabel->setText(Locale::tr(QStringLiteral("office.progress.file_downloading")));
        row.statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(accentHex));
        break;

    case OfficeDownloader::FileStatus::Verifying:
        row.iconLabel->setText(QStringLiteral("⚡"));
        row.iconLabel->setFont(Theme::sans(10.0, Theme::Weight::SemiBold));
        row.iconLabel->setStyleSheet(QStringLiteral("color: %1;").arg(purpleHex));
        row.statusLabel->setFont(Theme::sans(9.5, Theme::Weight::Medium));
        row.statusLabel->setText(Locale::tr(QStringLiteral("office.progress.file_verifying")));
        row.statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(purpleHex));
        break;

    case OfficeDownloader::FileStatus::Completed:
        row.iconLabel->setText(QStringLiteral("✓"));
        row.iconLabel->setFont(Theme::sans(11.0, Theme::Weight::SemiBold));
        row.iconLabel->setStyleSheet(QStringLiteral("color: %1;").arg(greenHex));
        if (!row.lastSha256.isEmpty()) {
            row.statusLabel->setText(QStringLiteral("SHA-256: %1").arg(row.lastSha256));
            row.statusLabel->setFont(Theme::mono(9.0));
            row.statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(greenHex));
        } else {
            row.statusLabel->clear();
        }
        break;

    case OfficeDownloader::FileStatus::Failed:
        row.iconLabel->setText(QStringLiteral("✕"));
        row.iconLabel->setFont(Theme::sans(11.0, Theme::Weight::SemiBold));
        row.iconLabel->setStyleSheet(QStringLiteral("color: %1;").arg(redHex));
        row.statusLabel->setFont(Theme::sans(9.5));
        row.statusLabel->setText(Locale::tr(QStringLiteral("office.progress.file_failed")));
        row.statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(redHex));
        break;
    }
}
