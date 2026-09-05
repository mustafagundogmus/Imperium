// downloadprogresswidget.h - Canlı indirme metriklerini ve kontrollerini gösteren panel.

#pragma once

#include "../officedownloader.h"

#include <QWidget>

class QLabel;
class QProgressBar;
class PillButton;
class QComboBox;
class QVBoxLayout;

class DownloadProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadProgressWidget(QWidget *parent = nullptr);

    void setDownloader(OfficeDownloader *downloader);
    void reset();

Q_SIGNALS:
    void openFolderRequested(const QString &filePath);
    void mountRequested(const QString &filePath);
    void closed();

public Q_SLOTS:
    void updateProgress(qint64 received, qint64 total, double speedMBps, int etaSeconds);
    void updateFileProgress(int completedFiles, int totalFiles, const QString &currentFileName,
                            qint64 fileReceived = 0, qint64 fileTotal = 0, double filePct = 0.0);
    void updateState(OfficeDownloader::State state);
    void initPackageFiles(const QStringList &fileNames, const QList<qint64> &fileSizes);
    void updatePackageFileStatus(int index, const QString &fileName, int status, qint64 size, const QString &sha256);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void retranslate();
    void updateMetricWidths();
    static QString formatSize(qint64 bytes);
    static QString formatEta(int seconds);

    struct FileRowWidgets {
        QWidget *rowWidget   = nullptr;
        QLabel  *iconLabel   = nullptr;
        QLabel  *nameLabel   = nullptr;
        QLabel  *sizeLabel   = nullptr;
        QLabel  *statusLabel = nullptr;
        int      lastStatus  = 0;   ///< son uygulanan FileStatus değeri
        QString  lastSha256;        ///< Completed durumunda gösterilen hash
    };

    OfficeDownloader *m_downloader = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_badgeLabel = nullptr;
    QLabel *m_partsBadge = nullptr;
    QLabel *m_pctLabel = nullptr;

    QProgressBar *m_progressBar = nullptr; // Toplam / Genel Çubuk

    QWidget *m_fileContainer = nullptr;
    QLabel *m_activeFileLabel = nullptr;
    QLabel *m_filePctLabel = nullptr;
    QProgressBar *m_fileProgressBar = nullptr; // Aktif Dosya Çubuğu

    QLabel *m_downloadedLabel = nullptr;
    QLabel *m_remainingLabel = nullptr;
    QLabel *m_speedLabel = nullptr;
    QLabel *m_etaLabel = nullptr;

    QWidget *m_hashContainer = nullptr;
    QLabel *m_hashLabel = nullptr;
    PillButton *m_copyHashBtn = nullptr;
    QString m_currentSha256;

    PillButton *m_pauseResumeBtn = nullptr;
    PillButton *m_cancelBtn = nullptr;
    PillButton *m_openFolderBtn = nullptr;
    PillButton *m_mountBtn = nullptr;
    PillButton *m_closeBtn = nullptr;
    QComboBox *m_toolCombo = nullptr;

    QString m_filePath;
    qint64 m_lastReceived = 0;
    qint64 m_lastTotal = 0;
    double m_lastSpeed = 0.0;
    int m_lastEta = 0;
    int m_completedFiles = 0;
    int m_totalFiles = 1;
    QString m_activeFileName;
    double m_lastFilePct = 0.0;
    qint64 m_lastFileReceived = 0;
    qint64 m_lastFileTotal = 0;
    OfficeDownloader::State m_lastState = OfficeDownloader::State::Idle;

    QWidget *m_packageFilesContainer = nullptr;
    QLabel *m_packageFilesHeader = nullptr;
    QWidget *m_fileListWidget = nullptr;
    QVBoxLayout *m_fileListLayout = nullptr;
    QList<FileRowWidgets> m_fileRows;
};
