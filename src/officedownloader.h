// officedownloader.h - Microsoft Office CDN üzerinden doğrudan asenkron dosya indirici.

#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

class OfficeDownloader : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Downloading,
        Paused,
        Verifying,
        Completed,
        Failed,
        Cancelled
    };
    Q_ENUM(State)

    enum class FileStatus {
        Pending = 0,
        Downloading = 1,
        Verifying = 2,
        Completed = 3,
        Failed = 4
    };
    Q_ENUM(FileStatus)

    explicit OfficeDownloader(QObject *parent = nullptr);
    ~OfficeDownloader() override;

    State state() const { return m_state; }
    QString currentFileName() const { return m_fileName; }
    QString targetFilePath() const { return m_finalPath; }
    QString currentTool() const { return m_tool; }
    QString lastSha256() const { return m_lastSha256; }
    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 bytesTotal() const { return m_bytesTotal; }
    double speedMBps() const { return m_speedMBps; }
    int etaSeconds() const { return m_etaSeconds; }
    int completedFiles() const { return m_completedFiles; }
    int totalFiles() const { return m_totalFiles; }
    QString activeFileName() const { return m_currentFileName; }
    bool isPackageMode() const { return m_isPackageMode; }

    /// Tekil dosya (örn. ISO) indirmesini başlatır. Tool: "aria2" veya "internal"
    void startDownload(const QString &url, const QString &outputDir, const QString &fileName,
                       const QString &tool = QStringLiteral("aria2"));

    /// Çevrimdışı kurulum paketi (C2R klasör yapısı) indirmesini başlatır.
    void startPackageDownload(const QString &channelGuid, const QString &channelName,
                              const QString &arch, const QString &lang,
                              const QString &outputDir,
                              const QString &tool = QStringLiteral("aria2"));

    /// İndirmeyi geçici olarak durdurur (resume edilebilir).
    void pause();

    /// Duraklatılmış indirmeyi kaldığı yerden devam ettirir.
    void resume();

    /// İndirmeyi iptal eder ve geçici dosyayı temizler.
    void cancel();

    /// İndirme aracını günceller.
    void setTool(const QString &tool);

    /// İndirme aracını çalışma anında veya duraklatılmışken güvenle değiştirir.
    void switchTool(const QString &newTool);

    /// Exe içine gömülü aria2c.exe dosyasını temp klasörüne çıkartır.
    static QString ensureAria2Extracted();
    /// Exe içine gömülü wget.exe dosyasını temp klasörüne çıkartır.
    static QString ensureWgetExtracted();

Q_SIGNALS:
    void stateChanged(OfficeDownloader::State state);
    void progress(qint64 received, qint64 total, double speedMBps, int etaSeconds);
    void fileProgress(int completedFiles, int totalFiles, const QString &currentFileName,
                      qint64 fileReceived = 0, qint64 fileTotal = 0, double filePct = 0.0);
    void packageFilesInitialized(const QStringList &fileNames, const QList<qint64> &fileSizes);
    void packageFileStatusChanged(int index, const QString &fileName, int status, qint64 size, const QString &sha256);
    void finished(bool success, const QString &filePath, const QString &error);
    void hashReady(const QString &filePath, const QString &sha256);
    void logMessage(const QString &message);

private Q_SLOTS:
    void onReadyRead();
    void onReplyFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onSpeedTimerTick();

    void onAriaOutput();
    void onAriaFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void onWgetOutput();
    void onWgetFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void onCurlOutput();
    void onCurlFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void startInternalRequest(qint64 startOffset = 0);
    void startInternalPackageDownload(const QStringList &remainingFiles);
    void downloadNextInternalPackageFile();
    void downloadNextPackageFile();
    void verifyAndAdvancePackageFile(const QString &relFile);
    void startAria2ForFile(const QString &url, const QString &targetDir, const QString &outName);
    void startWgetForFile(const QString &url, const QString &fullPath);
    void startCurlForFile(const QString &url, const QString &fullPath);
    void startAria2();
    void startWget();
    void startCurl();
    void resumePackageDownload();
    void finalizeDownload(const QString &toolName);
    QString ensureCurl();
    void cleanup();

    bool isPackageFileComplete(const QString &relPath, const QString &fullPath) const;
    bool validateOrCleanPackageFile(const QString &relPath, const QString &fullPath);
    void fetchPackageFileSizes(const QString &baseUrl, const QStringList &files, std::function<void()> onDone);
    void updatePackageProgressFromDisk();

    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_reply = nullptr;
    QProcess *m_ariaProcess = nullptr;
    QFile m_file;

    QString m_url;
    QString m_outputDir;
    QString m_fileName;
    QString m_partPath;
    QString m_finalPath;
    QString m_tool = QStringLiteral("aria2");

    bool m_isPackageMode = false;
    QString m_channelName;
    QString m_channelGuid;
    QString m_build;
    QString m_lang;
    QString m_archLabel;
    QString m_packageListFile;
    int m_completedFiles = 0;
    int m_packageFileIdx = 0;
    int m_totalFiles = 1;
    QString m_currentFileName;
    QStringList m_packageFiles;
    QStringList m_remainingPackageFiles;
    qint64 m_packageFileStartOffset = 0;
    QMap<QString, qint64> m_packageFileSizes;
    QMap<QString, QString> m_packageFileChecksums;
    bool m_isVerifyingFile = false;

    qint64 m_activeFileReceived = 0;
    qint64 m_activeFileTotal = 0;
    double m_activeFilePct = 0.0;
    qint64 m_totalPackageBytes = 0;

    State m_state = State::Idle;
    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = 0;
    qint64 m_startOffset = 0;

    QTimer m_speedTimer;
    QElapsedTimer m_elapsedTimer;
    qint64 m_lastBytesForSpeed = 0;
    qint64 m_lastElapsedMs = 0;
    double m_speedMBps = 0.0;
    int m_etaSeconds = 0;
    QString m_lastSha256;
    QString m_remoteETag;
};
