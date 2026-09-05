#include "officedownloader.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QUrl>

namespace {

qint64 parseUnitBytes(const QString &str)
{
    QString s = str.trimmed().toUpper();
    double mult = 1.0;
    if (s.endsWith(QLatin1String("GIB")) || s.endsWith(QLatin1String("GB"))) {
        mult = 1024.0 * 1024.0 * 1024.0;
        s.chop(s.endsWith(QLatin1String("GIB")) ? 3 : 2);
    } else if (s.endsWith(QLatin1String("G"))) {
        mult = 1024.0 * 1024.0 * 1024.0;
        s.chop(1);
    } else if (s.endsWith(QLatin1String("MIB")) || s.endsWith(QLatin1String("MB"))) {
        mult = 1024.0 * 1024.0;
        s.chop(s.endsWith(QLatin1String("MIB")) ? 3 : 2);
    } else if (s.endsWith(QLatin1String("M"))) {
        mult = 1024.0 * 1024.0;
        s.chop(1);
    } else if (s.endsWith(QLatin1String("KIB")) || s.endsWith(QLatin1String("KB"))) {
        mult = 1024.0;
        s.chop(s.endsWith(QLatin1String("KIB")) ? 3 : 2);
    } else if (s.endsWith(QLatin1String("K"))) {
        mult = 1024.0;
        s.chop(1);
    } else if (s.endsWith(QLatin1String("B"))) {
        s.chop(1);
    }
    s = s.trimmed();
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    return qint64(s.toDouble() * mult);
}

int parseEtaSeconds(const QString &str)
{
    const QString s = str.trimmed();
    if (s.isEmpty())
        return 0;

    // cURL format: "HH:MM:SS" veya "MM:SS" (ör. "1:07:17", "00:01", "--:--")
    if (s.contains(QLatin1Char(':'))) {
        if (s.contains(QLatin1Char('-')))
            return 0; // "--:--" henüz bilinmiyor
        const QStringList parts = s.split(QLatin1Char(':'));
        if (parts.size() == 3) {
            return parts[0].toInt() * 3600 + parts[1].toInt() * 60 + parts[2].toInt();
        }
        if (parts.size() == 2) {
            return parts[0].toInt() * 60 + parts[1].toInt();
        }
        return 0;
    }

    // aria2 / wget format: "1h7m17s", "5m30s", "45s"
    int totalSec = 0;
    int curNum = 0;
    for (const QChar c : s) {
        if (c.isDigit()) {
            curNum = curNum * 10 + c.digitValue();
        } else if (c == QLatin1Char('h') || c == QLatin1Char('H')) {
            totalSec += curNum * 3600;
            curNum = 0;
        } else if (c == QLatin1Char('m') || c == QLatin1Char('M')) {
            totalSec += curNum * 60;
            curNum = 0;
        } else if (c == QLatin1Char('s') || c == QLatin1Char('S')) {
            totalSec += curNum;
            curNum = 0;
        }
    }
    return totalSec + curNum;
}

int resolveLcid(const QString &lang)
{
    static const QHash<QString, int> lcidMap = {
        {QStringLiteral("tr-tr"), 1055},
        {QStringLiteral("en-us"), 1033},
        {QStringLiteral("en-gb"), 2057},
        {QStringLiteral("de-de"), 1031},
        {QStringLiteral("fr-fr"), 1036},
        {QStringLiteral("es-es"), 3082},
        {QStringLiteral("it-it"), 1040},
        {QStringLiteral("pt-br"), 1046},
        {QStringLiteral("pt-pt"), 2070},
        {QStringLiteral("pl-pl"), 1045},
        {QStringLiteral("ru-ru"), 1049},
        {QStringLiteral("ar-sa"), 1025},
        {QStringLiteral("zh-cn"), 2052},
        {QStringLiteral("zh-tw"), 1028},
        {QStringLiteral("ja-jp"), 1041},
        {QStringLiteral("ko-kr"), 1042},
        {QStringLiteral("nl-nl"), 1043},
        {QStringLiteral("sv-se"), 1053},
        {QStringLiteral("da-dk"), 1030},
        {QStringLiteral("fi-fi"), 1035},
        {QStringLiteral("nb-no"), 1044},
        {QStringLiteral("cs-cz"), 1029},
        {QStringLiteral("el-gr"), 1032},
        {QStringLiteral("hu-hu"), 1038},
        {QStringLiteral("ro-ro"), 1048},
        {QStringLiteral("uk-ua"), 1058},
        {QStringLiteral("vi-vn"), 1066},
        {QStringLiteral("th-th"), 1054},
        {QStringLiteral("hi-in"), 1081},
        {QStringLiteral("he-il"), 1037},
        {QStringLiteral("id-id"), 1057},
        {QStringLiteral("es-mx"), 2058},
        {QStringLiteral("fr-ca"), 3084},
        {QStringLiteral("sk-sk"), 1051}
    };
    return lcidMap.value(lang.toLower(), 1033);
}

} // namespace

OfficeDownloader::OfficeDownloader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(&m_speedTimer, &QTimer::timeout, this, &OfficeDownloader::onSpeedTimerTick);
}

OfficeDownloader::~OfficeDownloader()
{
    cleanup();
}

QString OfficeDownloader::ensureAria2Extracted()
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Arbitrium");
    QDir().mkpath(tempDir);
    const QString ariaPath = QDir(tempDir).filePath(QStringLiteral("aria2c.exe"));

    QFile resFile(QStringLiteral(":/bin/aria2c.exe"));
    if (resFile.exists()) {
        const qint64 resSize = resFile.size();
        if (!QFileInfo::exists(ariaPath) || QFileInfo(ariaPath).size() != resSize) {
            if (QFile::exists(ariaPath))
                QFile::remove(ariaPath);
            if (resFile.open(QIODevice::ReadOnly)) {
                QFile outFile(ariaPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(resFile.readAll());
                    outFile.close();
                }
                resFile.close();
            }
        }
    }
    return ariaPath;
}

QString OfficeDownloader::ensureWgetExtracted()
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Arbitrium");
    QDir().mkpath(tempDir);
    const QString wgetPath = QDir(tempDir).filePath(QStringLiteral("wget.exe"));

    QFile resFile(QStringLiteral(":/bin/wget.exe"));
    if (resFile.exists()) {
        const qint64 resSize = resFile.size();
        if (!QFileInfo::exists(wgetPath) || QFileInfo(wgetPath).size() != resSize) {
            if (QFile::exists(wgetPath))
                QFile::remove(wgetPath);
            if (resFile.open(QIODevice::ReadOnly)) {
                QFile outFile(wgetPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(resFile.readAll());
                    outFile.close();
                }
                resFile.close();
            }
        }
    }
    return wgetPath;
}

QString OfficeDownloader::ensureCurl()
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Arbitrium");
    QDir().mkpath(tempDir);
    const QString curlPath = QDir(tempDir).filePath(QStringLiteral("curl.exe"));

    QFile resFile(QStringLiteral(":/bin/curl.exe"));
    if (resFile.exists()) {
        const qint64 resSize = resFile.size();
        if (!QFileInfo::exists(curlPath) || QFileInfo(curlPath).size() != resSize) {
            if (QFile::exists(curlPath))
                QFile::remove(curlPath);
            if (resFile.open(QIODevice::ReadOnly)) {
                QFile outFile(curlPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(resFile.readAll());
                    outFile.close();
                }
                resFile.close();
            }
        }
        return curlPath;
    }

    // Sistemdeki curl.exe'yi ara (Windows 10/11 C:\Windows\System32\curl.exe veya PATH)
    const QString sysCurl = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (!sysCurl.isEmpty() && QFileInfo::exists(sysCurl)) {
        return sysCurl;
    }
    const QString win32Curl = QStringLiteral("C:/Windows/System32/curl.exe");
    if (QFileInfo::exists(win32Curl)) {
        return win32Curl;
    }

    return QString();
}

bool OfficeDownloader::isPackageFileComplete(const QString &relPath, const QString &fullPath) const
{
    if (!QFile::exists(fullPath))
        return false;

    // Aria2 kontrol dosyası varsa indirme kesinlikle devam ediyordur -> tamamlanmamış
    if (QFile::exists(fullPath + QStringLiteral(".aria2")))
        return false;

    const qint64 diskSize = QFileInfo(fullPath).size();
    if (diskSize <= 512)
        return false;

    // Microsoft CDN'den alınan Content-Length ile birebir bayt kontrolü
    const qint64 expectedSize = m_packageFileSizes.value(relPath, -1);
    if (expectedSize > 0) {
        if (diskSize == expectedSize) {
            // CAB dosyaları için Magic Bytes ("MSCF") kontrolü
            if (relPath.endsWith(QStringLiteral(".cab"), Qt::CaseInsensitive)) {
                QFile f(fullPath);
                if (f.open(QIODevice::ReadOnly)) {
                    const QByteArray header = f.read(4);
                    f.close();
                    if (header != "MSCF") {
                        return false;
                    }
                }
            }
            return true;
        }

        // Özel durum: v64.cab ve v32.cab dosyaları finalizeDownload aşamasında
        // v64_<build>.cab veya v32_<build>.cab kopyalanarak oluşturulur.
        if (relPath == QStringLiteral("v64.cab")) {
            const qint64 vVerExpected = m_packageFileSizes.value(QStringLiteral("v64_%1.cab").arg(m_build), -1);
            if (vVerExpected > 0 && diskSize == vVerExpected)
                return true;
        } else if (relPath == QStringLiteral("v32.cab")) {
            const qint64 vVerExpected = m_packageFileSizes.value(QStringLiteral("v32_%1.cab").arg(m_build), -1);
            if (vVerExpected > 0 && diskSize == vVerExpected)
                return true;
        }

        return false;
    }

    return false;
}

bool OfficeDownloader::validateOrCleanPackageFile(const QString &relPath, const QString &fullPath)
{
    if (!QFile::exists(fullPath)) {
        QFile::remove(fullPath + QStringLiteral(".aria2"));
        return false;
    }

    const qint64 diskSize = QFileInfo(fullPath).size();
    const qint64 expectedSize = m_packageFileSizes.value(relPath, -1);

    // 1. Çok küçük / boş dosya kontrolü
    if (diskSize <= 512) {
        QFile::remove(fullPath);
        QFile::remove(fullPath + QStringLiteral(".aria2"));
        return false;
    }

    // 2. Beklenen boyuttan büyük dosya kontrolü (HTTP 416 ve motor reddini önler)
    if (expectedSize > 0 && diskSize > expectedSize) {
        Q_EMIT logMessage(QStringLiteral("[WARN] %1 dosyasının boyutu (%2 byte) beklenen boyuttan (%3 byte) büyük, bozuk dosya temizleniyor...")
                          .arg(relPath).arg(diskSize).arg(expectedSize));
        QFile::remove(fullPath);
        QFile::remove(fullPath + QStringLiteral(".aria2"));
        return false;
    }

    // 3. CAB dosyaları için Magic Bytes ("MSCF") başlık bütünlüğü kontrolü
    if (relPath.endsWith(QStringLiteral(".cab"), Qt::CaseInsensitive) && diskSize >= 4) {
        QFile f(fullPath);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray header = f.read(4);
            f.close();
            if (header != "MSCF") {
                Q_EMIT logMessage(QStringLiteral("[WARN] %1 CAB dosyasının başlığı bozuk ('MSCF' değil), temizleniyor...")
                                  .arg(relPath));
                QFile::remove(fullPath);
                QFile::remove(fullPath + QStringLiteral(".aria2"));
                return false;
            }
        }
    }

    // 4. Dosya tam boyuta ulaşmış mı?
    if (expectedSize > 0 && diskSize == expectedSize) {
        QFile::remove(fullPath + QStringLiteral(".aria2"));
        return true;
    }

    // 5. v64.cab ve v32.cab için özel durum
    if (relPath == QStringLiteral("v64.cab")) {
        const qint64 vVerExpected = m_packageFileSizes.value(QStringLiteral("v64_%1.cab").arg(m_build), -1);
        if (vVerExpected > 0 && diskSize == vVerExpected) {
            QFile::remove(fullPath + QStringLiteral(".aria2"));
            return true;
        }
    } else if (relPath == QStringLiteral("v32.cab")) {
        const qint64 vVerExpected = m_packageFileSizes.value(QStringLiteral("v32_%1.cab").arg(m_build), -1);
        if (vVerExpected > 0 && diskSize == vVerExpected) {
            QFile::remove(fullPath + QStringLiteral(".aria2"));
            return true;
        }
    }

    // 6. Dosya kısmi inmiş ve geçerli: Aria2 harici motorlarda .aria2 dosyasını temizle
    if (m_tool != QStringLiteral("aria2")) {
        QFile::remove(fullPath + QStringLiteral(".aria2"));
    }
    return false;
}

void OfficeDownloader::fetchPackageFileSizes(const QString &baseUrl, const QStringList &files, std::function<void()> onDone)
{
    if (files.isEmpty()) {
        if (onDone) onDone();
        return;
    }

    struct FetchState {
        int pending = 0;
        bool completed = false;
        QTimer *timeoutTimer = nullptr;
    };

    auto state = QSharedPointer<FetchState>::create();
    state->pending = files.size();

    auto finishFetch = [this, state, onDone]() {
        if (state->completed)
            return;
        state->completed = true;
        if (state->timeoutTimer) {
            state->timeoutTimer->stop();
            state->timeoutTimer->deleteLater();
            state->timeoutTimer = nullptr;
        }
        if (onDone) {
            onDone();
        }
    };

    // 4 saniye zaman aşımı emniyeti
    state->timeoutTimer = new QTimer(this);
    state->timeoutTimer->setSingleShot(true);
    connect(state->timeoutTimer, &QTimer::timeout, this, [finishFetch]() {
        finishFetch();
    });
    state->timeoutTimer->start(4000);

    for (const QString &f : files) {
        const QUrl fileUrl(baseUrl + QLatin1Char('/') + f);
        QNetworkRequest req(fileUrl);
        req.setRawHeader("User-Agent", "Arbitrium/1.0 (Windows NT; x64)");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *headReply = m_nam->head(req);
        connect(headReply, &QNetworkReply::finished, this, [this, headReply, f, state, finishFetch]() {
            headReply->deleteLater();
            if (headReply->error() == QNetworkReply::NoError) {
                const qint64 len = headReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
                if (len > 0) {
                    m_packageFileSizes[f] = len;
                }
            }
            state->pending--;
            if (state->pending <= 0) {
                finishFetch();
            }
        });
    }
}

void OfficeDownloader::updatePackageProgressFromDisk()
{
    if (!m_isPackageMode || m_packageFiles.isEmpty())
        return;

    const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
    int completedCount = 0;
    qint64 completedBytes = 0;
    int activeIdx = -1;

    for (int i = 0; i < m_packageFiles.size(); ++i) {
        const QString rel = m_packageFiles[i];
        const QString full = QDir(dataDir).filePath(rel);
        if (isPackageFileComplete(rel, full)) {
            completedCount++;
            completedBytes += m_packageFileSizes.value(rel, QFileInfo(full).size());
        } else if (activeIdx < 0) {
            activeIdx = i;
        }
    }

    m_completedFiles = completedCount;
    m_packageFileIdx = completedCount;

    if (activeIdx < 0) {
        // Tüm dosyalar diskte eksiksiz tamamlanmış
        m_currentFileName = QFileInfo(m_packageFiles.last()).fileName();
        m_activeFileTotal = m_packageFileSizes.value(m_packageFiles.last(), 0);
        m_activeFileReceived = m_activeFileTotal;
        m_activeFilePct = 100.0;
        m_bytesReceived = (m_totalPackageBytes > 0) ? m_totalPackageBytes : completedBytes;
    } else {
        const QString activeRel = m_packageFiles[activeIdx];
        m_currentFileName = QFileInfo(activeRel).fileName();
        const QString activeFull = QDir(dataDir).filePath(activeRel);
        const qint64 activeExpected = m_packageFileSizes.value(activeRel, 0);
        if (activeExpected > 0) {
            m_activeFileTotal = activeExpected;
        }

        const qint64 diskSize = QFile::exists(activeFull) ? QFileInfo(activeFull).size() : 0;
        if (diskSize > m_activeFileReceived) {
            m_activeFileReceived = diskSize;
        }
        if (m_activeFileTotal > 0) {
            m_activeFilePct = (double(m_activeFileReceived) / double(m_activeFileTotal)) * 100.0;
        }
        m_bytesReceived = completedBytes + m_activeFileReceived;
    }

    if (m_totalPackageBytes <= 0) {
        qint64 totalCalculated = 0;
        for (const QString &f : m_packageFiles) {
            totalCalculated += m_packageFileSizes.value(f, 0);
        }
        if (totalCalculated > 0) {
            m_totalPackageBytes = totalCalculated;
        } else if (m_archLabel.contains(QStringLiteral("86_x64")) || m_archLabel.contains(QStringLiteral("Multi"))) {
            m_totalPackageBytes = 4900LL * 1024LL * 1024LL;
        } else if (m_archLabel.contains(QStringLiteral("x86"))) {
            m_totalPackageBytes = 2500LL * 1024LL * 1024LL;
        } else {
            m_totalPackageBytes = 2950LL * 1024LL * 1024LL;
        }
    }
    m_bytesTotal = qMax(m_totalPackageBytes, m_bytesReceived);
}

void OfficeDownloader::cleanup()
{
    m_speedTimer.stop();
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->waitForFinished(2000);
        delete m_ariaProcess;
        m_ariaProcess = nullptr;
    }
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
    m_remainingPackageFiles.clear();
}

void OfficeDownloader::startDownload(const QString &url, const QString &outputDir, const QString &fileName, const QString &tool)
{
    cleanup();

    m_isPackageMode = false;
    m_packageFiles.clear();
    m_completedFiles = 0;
    m_totalFiles = 1;
    m_currentFileName = fileName;
    m_activeFileReceived = 0;
    m_activeFileTotal = 0;
    m_activeFilePct = 0.0;
    m_totalPackageBytes = 0;
    m_lastSha256.clear();
    Q_EMIT fileProgress(0, 1, fileName, 0, 0, 0.0);

    m_url = url;
    m_outputDir = outputDir;
    m_fileName = fileName;
    m_tool = tool.toLower();

    QDir().mkpath(outputDir);
    m_finalPath = QDir(outputDir).filePath(fileName);
    m_partPath = m_finalPath + QStringLiteral(".part");

    m_startOffset = 0;
    const QString ariaControl = m_partPath + QStringLiteral(".aria2");
    if (QFileInfo::exists(ariaControl)) {
        QFile::remove(ariaControl);
    }
    if (QFileInfo::exists(m_partPath)) {
        const qint64 pSize = QFileInfo(m_partPath).size();
        if (pSize <= 512) {
            QFile::remove(m_partPath);
        } else {
            m_startOffset = pSize;
        }
    }
    m_bytesReceived = m_startOffset;
    m_lastBytesForSpeed = m_startOffset;
    m_bytesTotal = 0;
    m_speedMBps = 0.0;
    m_etaSeconds = 0;
    m_lastElapsedMs = 0;
    m_elapsedTimer.start();

    // Varsa mevcut indirme durumunu arayüze hemen yansıt
    if (m_bytesReceived > 0) {
        Q_EMIT progress(m_bytesReceived, m_bytesTotal, 0.0, 0);
    }

    m_remoteETag.clear();

    // Tekil dosya boyutunu baştan netleştirmek ve ETag doğrulamak için hızlı HEAD isteği
    QNetworkRequest headReq((QUrl(url)));
    headReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    headReq.setRawHeader("User-Agent", "Arbitrium/1.0 (Windows NT; x64)");
    QNetworkReply *headReply = m_nam->head(headReq);
    connect(headReply, &QNetworkReply::finished, this, [this, headReply]() {
        headReply->deleteLater();
        if (headReply->error() == QNetworkReply::NoError) {
            const QByteArray remoteEtag = headReply->rawHeader("ETag").trimmed();
            const QString etagPath = m_partPath + QStringLiteral(".etag");
            if (!remoteEtag.isEmpty()) {
                if (QFile::exists(etagPath) && QFile::exists(m_partPath)) {
                    QFile ef(etagPath);
                    if (ef.open(QIODevice::ReadOnly)) {
                        const QByteArray savedEtag = ef.readAll().trimmed();
                        ef.close();
                        if (!savedEtag.isEmpty() && savedEtag != remoteEtag) {
                            Q_EMIT logMessage(QStringLiteral("[WARN] Sunucudaki dosya sürümü güncellenmiş (ETag değişmiş). Kısmi indirme sıfırlanıyor..."));
                            cleanup();
                            QFile::remove(m_partPath);
                            QFile::remove(m_partPath + QStringLiteral(".aria2"));
                            QFile::remove(etagPath);
                            m_startOffset = 0;
                            m_bytesReceived = 0;
                            if (m_state == State::Downloading) {
                                resume();
                                return;
                            }
                        }
                    }
                }
                m_remoteETag = QString::fromUtf8(remoteEtag);
                QFile ef(etagPath);
                if (ef.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    ef.write(remoteEtag);
                    ef.close();
                }
            }

            const QVariant lenHeader = headReply->header(QNetworkRequest::ContentLengthHeader);
            if (lenHeader.isValid()) {
                const qint64 total = lenHeader.toLongLong();
                if (total > 0) {
                    m_bytesTotal = total;
                    // Diskteki kısmi dosya sunucu boyutunu aşmışsa bozuktur -> temizle!
                    if (QFile::exists(m_partPath)) {
                        const qint64 partSize = QFileInfo(m_partPath).size();
                        if (partSize > total) {
                            Q_EMIT logMessage(QStringLiteral("[WARN] Diskteki kısmi dosya boyutu (%1 byte) sunucu boyutunu (%2 byte) aşıyor, bozuk dosya temizleniyor...")
                                              .arg(partSize).arg(total));
                            cleanup();
                            QFile::remove(m_partPath);
                            QFile::remove(m_partPath + QStringLiteral(".aria2"));
                            QFile::remove(etagPath);
                            m_startOffset = 0;
                            m_bytesReceived = 0;
                            if (m_state == State::Downloading) {
                                resume();
                                return;
                            }
                        }
                    }
                    Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);
                }
            }
        }
    });

    if (m_tool == QStringLiteral("aria2")) {
        startAria2();
    } else if (m_tool == QStringLiteral("wget")) {
        startWget();
    } else if (m_tool == QStringLiteral("curl")) {
        startCurl();
    } else {
        startInternalRequest(m_startOffset);
    }
}

void OfficeDownloader::startPackageDownload(const QString &channelGuid, const QString &channelName,
                                           const QString &arch, const QString &lang,
                                           const QString &outputDir, const QString &tool)
{
    cleanup();

    m_isPackageMode = true;
    m_channelGuid = channelGuid;
    m_channelName = channelName;
    m_lang = lang;
    m_archLabel = (arch == QStringLiteral("Multi")) ? QStringLiteral("x86_x64") : arch;
    m_outputDir = outputDir;
    m_tool = tool.toLower();

    m_fileName = QStringLiteral("Office Package - %1").arg(m_channelName);
    m_bytesReceived = 0;
    m_bytesTotal = 0;
    m_speedMBps = 0.0;
    m_etaSeconds = 0;
    m_lastBytesForSpeed = 0;
    m_lastElapsedMs = 0;
    m_elapsedTimer.start();
    m_completedFiles = 0;
    m_totalFiles = 1;
    m_activeFileReceived = 0;
    m_activeFileTotal = 0;
    m_activeFilePct = 0.0;
    m_lastSha256.clear();
    if (m_archLabel.contains(QStringLiteral("86_x64")) || m_archLabel.contains(QStringLiteral("Multi"))) {
        m_totalPackageBytes = 5600LL * 1024LL * 1024LL;
    } else if (m_archLabel.contains(QStringLiteral("x86"))) {
        m_totalPackageBytes = 2800LL * 1024LL * 1024LL;
    } else {
        m_totalPackageBytes = 3650LL * 1024LL * 1024LL;
    }
    m_bytesTotal = m_totalPackageBytes;
    m_state = State::Downloading;
    Q_EMIT stateChanged(m_state);

    Q_EMIT logMessage(QStringLiteral("[INFO] Microsoft CDN'den en son sürüm bilgisi alınıyor..."));

    // Query AvailableBuild
    const QUrl buildUrl(QStringLiteral("https://mrodevicemgr.officeapps.live.com/mrodevicemgrsvc/api/v2/C2RReleaseData?audienceFFN=") + channelGuid);
    QNetworkRequest req(buildUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "Arbitrium/1.0 (Windows NT; x64)");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, arch]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_state = State::Failed;
            Q_EMIT stateChanged(m_state);
            Q_EMIT finished(false, QString(), QStringLiteral("Sürüm bilgisi alınamadı: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        m_build = doc.object().value(QStringLiteral("AvailableBuild")).toString();
        if (m_build.isEmpty()) {
            m_state = State::Failed;
            Q_EMIT stateChanged(m_state);
            Q_EMIT finished(false, QString(), QStringLiteral("Geçerli bir sürüm numarası bulunamadı."));
            return;
        }

        Q_EMIT logMessage(QStringLiteral("[INFO] En son sürüm bulundu: %1").arg(m_build));

        // Create directory structure
        QString safeLang = m_lang;
        safeLang.replace(' ', '_');
        QString safeChannel = m_channelName;
        safeChannel.replace(' ', '_');
        const QString dirName = QStringLiteral("%1_Office_%2_%3_v%4")
                                    .arg(safeLang, safeChannel, m_archLabel, m_build);
        m_finalPath = QDir(m_outputDir).filePath(dirName);

        const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
        const QString buildDir = dataDir + QStringLiteral("/") + m_build;
        QDir().mkpath(buildDir);

        // LCID
        const int lcid = resolveLcid(m_lang);

        // Files list
        QStringList files;
        const QStringList archs = (arch == QStringLiteral("Multi")) ? QStringList{QStringLiteral("x86"), QStringLiteral("x64")}
                                                                    : QStringList{arch};
        for (const auto &a : archs) {
            const QString bit = (a == QStringLiteral("x86")) ? QStringLiteral("32") : QStringLiteral("64");
            const QString bit0 = bit + QLatin1Char('0');
            files << QStringLiteral("v%1.cab").arg(bit);
            files << QStringLiteral("v%1_%2.cab").arg(bit, m_build);
            files << QStringLiteral("%1/i%2.cab").arg(m_build, bit0);
            files << QStringLiteral("%1/i%2%3.cab").arg(m_build, bit, QString::number(lcid));
            files << QStringLiteral("%1/s%2.cab").arg(m_build, bit0);
            files << QStringLiteral("%1/s%2%3.cab").arg(m_build, bit, QString::number(lcid));
            files << QStringLiteral("%1/sp%2%3.cab").arg(m_build, bit, QString::number(lcid));
            files << QStringLiteral("%1/stream.%2.%3.dat").arg(m_build, a, m_lang);
            files << QStringLiteral("%1/stream.%2.x-none.dat").arg(m_build, a);
            if (a == QStringLiteral("x86") && arch != QStringLiteral("Multi")) {
                files << QStringLiteral("%1/i640.cab").arg(m_build);
                files << QStringLiteral("%1/i64%2.cab").arg(m_build, QString::number(lcid));
            }
        }

        m_packageFiles = files;
        m_totalFiles = files.size();
        m_packageFileSizes.clear();

        const QString baseUrl = QStringLiteral("https://officecdn.microsoft.com/db/%1/Office/Data").arg(m_channelGuid);
        Q_EMIT logMessage(QStringLiteral("[INFO] Microsoft CDN'den paket dosya boyutları sorgulanıyor (%1 dosya)...").arg(files.size()));

        fetchPackageFileSizes(baseUrl, files, [this, baseUrl, dataDir, files]() {
            if (m_state != State::Downloading)
                return;

            qint64 totalCalculated = 0;
            for (const QString &f : files) {
                totalCalculated += m_packageFileSizes.value(f, 0);
            }
            if (totalCalculated > 0) {
                m_totalPackageBytes = totalCalculated;
                m_bytesTotal = totalCalculated;
            }

            // Disk kontrolü: Önceden indirilmiş tamamlanmış veya yarım kalmış dosyaları tespit et
            QStringList remainingFiles;
            QStringList fileNames;
            QList<qint64> fileSizes;

            for (int i = 0; i < files.size(); ++i) {
                const QString rel = files[i];
                fileNames << QFileInfo(rel).fileName();
                const QString full = QDir(dataDir).filePath(rel);
                qint64 sz = m_packageFileSizes.value(rel, 0);
                if (sz <= 0 && QFile::exists(full)) {
                    sz = QFileInfo(full).size();
                }
                fileSizes << sz;

                const bool isComplete = validateOrCleanPackageFile(rel, full);
                if (!isComplete) {
                    remainingFiles << rel;
                }
            }

            // Dosya listesi sinyali gönder
            Q_EMIT packageFilesInitialized(fileNames, fileSizes);

            for (int i = 0; i < files.size(); ++i) {
                const QString rel = files[i];
                const QString full = QDir(dataDir).filePath(rel);
                const qint64 actualSz = (fileSizes[i] > 0) ? fileSizes[i] : (QFile::exists(full) ? QFileInfo(full).size() : 0);
                if (isPackageFileComplete(rel, full)) {
                    Q_EMIT packageFileStatusChanged(i, fileNames[i], static_cast<int>(FileStatus::Completed),
                                                    actualSz, m_packageFileChecksums.value(rel));
                } else {
                    Q_EMIT packageFileStatusChanged(i, fileNames[i], static_cast<int>(FileStatus::Pending),
                                                    actualSz, QString());
                }
            }

            m_remainingPackageFiles = remainingFiles;
            updatePackageProgressFromDisk();
            m_lastBytesForSpeed = m_bytesReceived;

            Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                                m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
            if (m_bytesReceived > 0) {
                Q_EMIT progress(m_bytesReceived, m_bytesTotal, 0.0, 0);
            }

            if (remainingFiles.isEmpty()) {
                finalizeDownload(m_tool);
                return;
            }

            downloadNextPackageFile();
        });
    });
}

void OfficeDownloader::downloadNextPackageFile()
{
    if (m_state != State::Downloading)
        return;

    if (m_remainingPackageFiles.isEmpty()) {
        finalizeDownload(m_tool);
        return;
    }

    const QString relFile = m_remainingPackageFiles.first();
    const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
    const QString fullPath = QDir(dataDir).filePath(relFile);
    const QString fileName = QFileInfo(relFile).fileName();
    const int fileIdx = m_packageFiles.indexOf(relFile);

    QFileInfo(fullPath).dir().mkpath(QStringLiteral("."));

    const qint64 expectedSize = m_packageFileSizes.value(relFile, 0);
    const qint64 currentDiskSize = QFile::exists(fullPath) ? QFileInfo(fullPath).size() : 0;

    if (isPackageFileComplete(relFile, fullPath)) {
        verifyAndAdvancePackageFile(relFile);
        return;
    }

    m_currentFileName = fileName;
    m_packageFileStartOffset = currentDiskSize;
    m_activeFileReceived = currentDiskSize;
    m_activeFileTotal = expectedSize;
    m_activeFilePct = (expectedSize > 0) ? (double(currentDiskSize) / double(expectedSize) * 100.0) : 0.0;

    Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                        m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
    Q_EMIT packageFileStatusChanged(fileIdx, fileName, static_cast<int>(FileStatus::Downloading),
                                    expectedSize, QString());

    const QString fileUrl = QStringLiteral("https://officecdn.microsoft.com/db/%1/Office/Data/%2")
                                .arg(m_channelGuid, relFile);
    const QString targetDir = QFileInfo(fullPath).dir().absolutePath();

    m_speedTimer.start(500);

    if (m_tool == QStringLiteral("aria2")) {
        startAria2ForFile(fileUrl, targetDir, fileName);
    } else if (m_tool == QStringLiteral("wget")) {
        startWgetForFile(fileUrl, fullPath);
    } else if (m_tool == QStringLiteral("curl")) {
        startCurlForFile(fileUrl, fullPath);
    } else {
        // internal
        m_file.setFileName(fullPath);
        const auto openMode = (m_packageFileStartOffset > 0) ? (QIODevice::WriteOnly | QIODevice::Append)
                                                             : (QIODevice::WriteOnly | QIODevice::Truncate);
        if (!m_file.open(openMode)) {
            m_state = State::Failed;
            Q_EMIT stateChanged(m_state);
            Q_EMIT finished(false, QString(), QStringLiteral("Paket dosyası açılamadı: %1 (%2)").arg(relFile, m_file.errorString()));
            return;
        }

        QNetworkRequest req((QUrl(fileUrl)));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setRawHeader("User-Agent", "Arbitrium/1.0 (Windows NT; x64)");
        if (m_packageFileStartOffset > 0) {
            req.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(m_packageFileStartOffset) + "-");
        }

        if (m_reply) {
            m_reply->disconnect(this);
            m_reply->abort();
            m_reply->deleteLater();
            m_reply = nullptr;
        }

        m_reply = m_nam->get(req);
        connect(m_reply, &QNetworkReply::readyRead, this, &OfficeDownloader::onReadyRead);
        connect(m_reply, &QNetworkReply::downloadProgress, this, &OfficeDownloader::onDownloadProgress);
        connect(m_reply, &QNetworkReply::finished, this, &OfficeDownloader::onReplyFinished);
    }
}

void OfficeDownloader::verifyAndAdvancePackageFile(const QString &relFile)
{
    const int fileIdx = m_packageFiles.indexOf(relFile);
    const QString fileName = QFileInfo(relFile).fileName();
    const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
    const QString fullPath = QDir(dataDir).filePath(relFile);
    const qint64 fileSize = QFileInfo(fullPath).size();

    // 1. Durumu Verifying (Doğrulanıyor) yap ve UI'a bildir
    m_isVerifyingFile = true;
    m_speedTimer.stop();
    m_speedMBps = 0.0;
    m_etaSeconds = 0;

    Q_EMIT packageFileStatusChanged(fileIdx, fileName, static_cast<int>(FileStatus::Verifying),
                                    fileSize, QString());
    Q_EMIT logMessage(QStringLiteral("[INFO] %1 indirildi. Bütünlük kontrolü için SHA-256 sağlama toplamı hesaplanıyor...").arg(fileName));

    // 2. Arka planda SHA-256 hesapla
    QThread *worker = QThread::create([this, relFile, fullPath, fileIdx, fileName, fileSize]() {
        QString sha256Hex;
        QFile f(fullPath);
        if (f.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            QByteArray buf(512 * 1024, Qt::Uninitialized);
            while (!f.atEnd()) {
                const qint64 rd = f.read(buf.data(), buf.size());
                if (rd > 0) {
                    hash.addData(QByteArrayView(buf.constData(), rd));
                }
            }
            f.close();
            sha256Hex = QString::fromLatin1(hash.result().toHex().toLower());
        }

        QMetaObject::invokeMethod(this, [this, relFile, fileIdx, fileName, fileSize, sha256Hex]() {
            m_isVerifyingFile = false;
            if (m_state != State::Downloading)
                return;

            if (sha256Hex.isEmpty()) {
                Q_EMIT logMessage(QStringLiteral("[ERROR] %1 dosyasının sağlama toplamı hesaplanamadı!").arg(fileName));
                Q_EMIT packageFileStatusChanged(fileIdx, fileName, static_cast<int>(FileStatus::Failed),
                                                fileSize, QString());
                m_state = State::Failed;
                Q_EMIT stateChanged(m_state);
                Q_EMIT finished(false, QString(), QStringLiteral("%1 sağlama toplamı hatası").arg(fileName));
                return;
            }

            m_packageFileChecksums[relFile] = sha256Hex;
            Q_EMIT logMessage(QStringLiteral("[OK] %1 doğrulandı (SHA-256: %2)").arg(fileName, sha256Hex.left(16) + QStringLiteral("...")));

            // Durumu Completed (Tamamlandı - ✅) yap
            Q_EMIT packageFileStatusChanged(fileIdx, fileName, static_cast<int>(FileStatus::Completed),
                                            fileSize, sha256Hex);

            if (!m_remainingPackageFiles.isEmpty() && m_remainingPackageFiles.first() == relFile) {
                m_remainingPackageFiles.removeFirst();
            }

            updatePackageProgressFromDisk();
            Q_EMIT fileProgress(m_completedFiles, m_totalFiles, fileName,
                                fileSize, fileSize, 100.0);
            Q_EMIT progress(m_bytesReceived, m_bytesTotal, 0.0, 0);

            // Ve ŞİMDİ bir sonraki dosyanın indirilmesine geç!
            downloadNextPackageFile();
        }, Qt::QueuedConnection);
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void OfficeDownloader::startAria2ForFile(const QString &url, const QString &targetDir, const QString &outName)
{
    const QString ariaExe = ensureAria2Extracted();
    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->deleteLater();
    }

    m_ariaProcess = new QProcess(this);
    connect(m_ariaProcess, &QProcess::readyReadStandardOutput, this, &OfficeDownloader::onAriaOutput);
    connect(m_ariaProcess, &QProcess::readyReadStandardError, this, &OfficeDownloader::onAriaOutput);
    connect(m_ariaProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OfficeDownloader::onAriaFinished);

    const QStringList args = {
        QStringLiteral("-d"), targetDir,
        QStringLiteral("-o"), outName,
        QStringLiteral("--check-certificate=false"),
        QStringLiteral("--continue=true"),
        QStringLiteral("--file-allocation=none"),
        QStringLiteral("--auto-file-renaming=false"),
        QStringLiteral("--allow-overwrite=true"),
        QStringLiteral("--disk-cache=64M"),
        QStringLiteral("--connect-timeout=15"),
        QStringLiteral("--timeout=15"),
        QStringLiteral("--max-tries=5"),
        QStringLiteral("--retry-wait=2"),
        QStringLiteral("--split=1"),
        QStringLiteral("--stream-piece-selector=inorder"),
        QStringLiteral("--summary-interval=1"),
        QStringLiteral("--user-agent=Arbitrium/1.0 (Windows NT; x64)"),
        url
    };

    Q_EMIT logMessage(QStringLiteral("[INFO] %1 Aria2 ile indiriliyor...").arg(outName));
    m_ariaProcess->start(ariaExe, args);
}

void OfficeDownloader::startWgetForFile(const QString &url, const QString &fullPath)
{
    const QString wgetExe = ensureWgetExtracted();
    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->deleteLater();
    }

    m_ariaProcess = new QProcess(this);
    connect(m_ariaProcess, &QProcess::readyReadStandardOutput, this, &OfficeDownloader::onWgetOutput);
    connect(m_ariaProcess, &QProcess::readyReadStandardError, this, &OfficeDownloader::onWgetOutput);
    connect(m_ariaProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OfficeDownloader::onWgetFinished);

    const QStringList args = {
        QStringLiteral("-O"), fullPath,
        QStringLiteral("--no-check-certificate"),
        QStringLiteral("-c"),
        QStringLiteral("--no-cache"),
        QStringLiteral("--timeout=15"),
        QStringLiteral("--waitretry=2"),
        QStringLiteral("--progress=bar:force:noscroll"),
        QStringLiteral("--tries=5"),
        QStringLiteral("--user-agent=Arbitrium/1.0 (Windows NT; x64)"),
        url
    };

    Q_EMIT logMessage(QStringLiteral("[INFO] %1 Wget ile indiriliyor...").arg(QFileInfo(fullPath).fileName()));
    m_ariaProcess->start(wgetExe, args);
}

void OfficeDownloader::startCurlForFile(const QString &url, const QString &fullPath)
{
    const QString curlExe = ensureCurl();
    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->deleteLater();
    }

    m_ariaProcess = new QProcess(this);
    connect(m_ariaProcess, &QProcess::readyReadStandardOutput, this, &OfficeDownloader::onCurlOutput);
    connect(m_ariaProcess, &QProcess::readyReadStandardError, this, &OfficeDownloader::onCurlOutput);
    connect(m_ariaProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OfficeDownloader::onCurlFinished);

    const QStringList args = {
        QStringLiteral("-#"),
        QStringLiteral("-q"),
        QStringLiteral("-L"),
        QStringLiteral("-k"),
        QStringLiteral("-C"), QStringLiteral("-"),
        QStringLiteral("--tcp-nodelay"),
        QStringLiteral("--connect-timeout"), QStringLiteral("15"),
        QStringLiteral("--retry"), QStringLiteral("5"),
        QStringLiteral("--retry-delay"), QStringLiteral("2"),
        QStringLiteral("--user-agent"), QStringLiteral("Arbitrium/1.0 (Windows NT; x64)"),
        QStringLiteral("-o"), fullPath,
        url
    };

    Q_EMIT logMessage(QStringLiteral("[INFO] %1 cURL ile indiriliyor...").arg(QFileInfo(fullPath).fileName()));
    m_ariaProcess->start(curlExe, args);
}

void OfficeDownloader::startInternalPackageDownload(const QStringList &remainingFiles)
{
    m_remainingPackageFiles = remainingFiles;
    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->waitForFinished(1000);
        delete m_ariaProcess;
        m_ariaProcess = nullptr;
    }
    m_state = State::Downloading;
    Q_EMIT stateChanged(m_state);
    downloadNextPackageFile();
}

void OfficeDownloader::downloadNextInternalPackageFile()
{
    downloadNextPackageFile();
}

void OfficeDownloader::startAria2()
{
    const QString ariaExe = ensureAria2Extracted();
    if (!QFileInfo::exists(ariaExe)) {
        Q_EMIT logMessage(QStringLiteral("[WARN] aria2c.exe çıkarılamadı, dahili indiriciye dönülüyor..."));
        m_tool = QStringLiteral("internal");
        startInternalRequest(0);
        return;
    }

    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->deleteLater();
    }

    m_ariaProcess = new QProcess(this);
    connect(m_ariaProcess, &QProcess::readyReadStandardOutput, this, &OfficeDownloader::onAriaOutput);
    connect(m_ariaProcess, &QProcess::readyReadStandardError, this, &OfficeDownloader::onAriaOutput);
    connect(m_ariaProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &OfficeDownloader::onAriaFinished);

    const QStringList args = {
        m_url,
        QStringLiteral("-d"), m_outputDir,
        QStringLiteral("-o"), m_fileName + QStringLiteral(".part"),
        QStringLiteral("--check-certificate=false"),
        QStringLiteral("--continue=true"),
        QStringLiteral("--file-allocation=none"),
        QStringLiteral("--auto-file-renaming=false"),
        QStringLiteral("--allow-overwrite=true"),
        QStringLiteral("--stream-piece-selector=inorder"),
        QStringLiteral("--disk-cache=64M"),
        QStringLiteral("--auto-save-interval=10"),
        QStringLiteral("--connect-timeout=15"),
        QStringLiteral("--timeout=15"),
        QStringLiteral("--max-tries=5"),
        QStringLiteral("--retry-wait=2"),
        QStringLiteral("--max-connection-per-server=1"),
        QStringLiteral("--split=1"),
        QStringLiteral("--summary-interval=1"),
        QStringLiteral("--user-agent=Arbitrium/1.0 (Windows NT; x64)")
    };

    m_state = State::Downloading;
    Q_EMIT stateChanged(m_state);
    Q_EMIT logMessage(QStringLiteral("[INFO] Aria2 doğrusal akış ile indirme başlatıldı: %1").arg(m_fileName));

    m_speedTimer.start(500);
    m_ariaProcess->start(ariaExe, args);
}

void OfficeDownloader::onAriaOutput()
{
    if (!m_ariaProcess)
        return;

    const QByteArray rawData = m_ariaProcess->readAllStandardOutput() + m_ariaProcess->readAllStandardError();
    const QString text = QString::fromUtf8(rawData);

    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);

    static const QRegularExpression regex(
        QStringLiteral(R"(\[#\w+\s+([0-9.]+[A-Za-z]+)\/([0-9.]+[A-Za-z]+)\(([0-9.]+)%\).*?DL:([0-9.]+[A-Za-z/]+)(?:.*?ETA:([0-9a-zA-Z]+))?)"));

    for (const QString &line : lines) {
        if (line.contains(QLatin1String("Download complete:")) ||
            line.contains(QLatin1String("Download has already completed:"))) {
            if (m_isPackageMode) {
                updatePackageProgressFromDisk();
                Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                                    m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
            }
        }

        const auto match = regex.match(line);
        if (match.hasMatch()) {
            const qint64 received = parseUnitBytes(match.captured(1));
            const qint64 total = parseUnitBytes(match.captured(2));
            const double speedMBps = parseUnitBytes(match.captured(4)) / 1048576.0;
            const int etaSec = match.captured(5).isEmpty() ? 0 : parseEtaSeconds(match.captured(5));

            if (speedMBps > 0.01) {
                m_speedMBps = speedMBps;
            }
            if (etaSec > 0) {
                m_etaSeconds = etaSec;
            }

            if (m_isPackageMode) {
                // Aktif dosya ilerlemesi
                if (received > m_activeFileReceived) {
                    m_activeFileReceived = received;
                }
                if (total > 0) {
                    m_activeFileTotal = total;
                }
                if (m_activeFileTotal > 0) {
                    m_activeFilePct = (double(m_activeFileReceived) / double(m_activeFileTotal)) * 100.0;
                }
                updatePackageProgressFromDisk();

                Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                                    m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
                Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);
            } else {
                // Tekil dosya ilerlemesi
                if (received > m_bytesReceived) {
                    m_bytesReceived = received;
                }
                if (total > 0 && (m_bytesTotal <= 0 || total > m_bytesTotal)) {
                    m_bytesTotal = total;
                }

                Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);
            }
        }
    }
}

void OfficeDownloader::finalizeDownload(const QString &toolName)
{
    m_speedTimer.stop();
    m_speedMBps = 0.0;
    m_etaSeconds = 0;

    if (m_isPackageMode) {
        const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
        QDir d(dataDir);
        for (const QString &entry : d.entryList({QStringLiteral("v32_*.cab")}, QDir::Files)) {
            QFile::remove(d.filePath(QStringLiteral("v32.cab")));
            QFile::copy(d.filePath(entry), d.filePath(QStringLiteral("v32.cab")));
        }
        for (const QString &entry : d.entryList({QStringLiteral("v64_*.cab")}, QDir::Files)) {
            QFile::remove(d.filePath(QStringLiteral("v64.cab")));
            QFile::copy(d.filePath(entry), d.filePath(QStringLiteral("v64.cab")));
        }

        QFile scriptRes(QStringLiteral(":/scripts/start_setup_c2r.cmd"));
        const QString targetScript = QDir(m_finalPath).filePath(QStringLiteral("start_setup.cmd"));
        if (scriptRes.open(QIODevice::ReadOnly)) {
            QFile outScript(targetScript);
            if (outScript.open(QIODevice::WriteOnly)) {
                outScript.write(scriptRes.readAll());
                outScript.close();
            }
            scriptRes.close();
        }

        if (!m_packageListFile.isEmpty()) {
            QFile::remove(m_packageListFile);
            m_packageListFile.clear();
        }

        m_state = State::Verifying;
        Q_EMIT stateChanged(m_state);
        Q_EMIT logMessage(QStringLiteral("[INFO] Paket indirildi. Dosyalar doğrulanıyor ve sağlama toplamları hesaplanıyor..."));

        const QString finalPath = m_finalPath;
        const QString channelName = m_channelName;
        const QString build = m_build;
        const QString lang = m_lang;
        const QString arch = m_archLabel;
        const QString channelGuid = m_channelGuid;
        const QString tool = toolName;
        const QStringList pkgFiles = m_packageFiles;
        const QMap<QString, QString> checksumsMap = m_packageFileChecksums;

        QThread *worker = QThread::create([this, finalPath, channelName, build, lang, arch, channelGuid, tool, pkgFiles, checksumsMap]() {
            const QString pkgDataDir = finalPath + QStringLiteral("/Office/Data");
            const QString checksumsPath = QDir(finalPath).filePath(QStringLiteral("checksums.sha256"));
            QFile sumsFile(checksumsPath);
            QByteArray sumsContent;
            QByteArray buf(512 * 1024, Qt::Uninitialized);

            for (const QString &relFile : pkgFiles) {
                QString h = checksumsMap.value(relFile);
                if (h.isEmpty()) {
                    const QString absPath = QDir(pkgDataDir).filePath(relFile);
                    if (QFile::exists(absPath)) {
                        QFile f(absPath);
                        if (f.open(QIODevice::ReadOnly)) {
                            QCryptographicHash hash(QCryptographicHash::Sha256);
                            while (!f.atEnd()) {
                                const qint64 rd = f.read(buf.data(), buf.size());
                                if (rd > 0) hash.addData(QByteArrayView(buf.constData(), rd));
                            }
                            f.close();
                            h = QString::fromLatin1(hash.result().toHex().toLower());
                        }
                    }
                }
                if (!h.isEmpty()) {
                    sumsContent.append(QStringLiteral("%1 *Office/Data/%2\n").arg(h, relFile).toUtf8());
                }
            }

            if (sumsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                sumsFile.write(sumsContent);
                sumsFile.close();
            }

            const QString manifestSha256 = QString::fromLatin1(QCryptographicHash::hash(sumsContent, QCryptographicHash::Sha256).toHex().toLower());

            const QString infoPath = QDir(finalPath).filePath(QStringLiteral("package.info"));
            QFile infoFile(infoPath);
            if (infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&infoFile);
                ts << "[Arbitrium Office Package Info]\n";
                ts << "Channel = " << channelName << "\n";
                ts << "Build = " << build << "\n";
                ts << "Language = " << lang << "\n";
                ts << "Architecture = " << arch << "\n";
                ts << "ChannelGuid = " << channelGuid << "\n";
                ts << "SHA256 = " << manifestSha256 << "\n";
                ts << "DownloadTool = " << tool << "\n";
                ts << "DownloadDate = " << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
                ts << "Status = Verified\n";
                infoFile.close();
            }

            QMetaObject::invokeMethod(this, [this, finalPath, tool, manifestSha256, checksumsPath]() {
                m_lastSha256 = manifestSha256;
                m_completedFiles = m_totalFiles;
                m_state = State::Completed;
                Q_EMIT stateChanged(m_state);
                Q_EMIT fileProgress(m_totalFiles, m_totalFiles, QStringLiteral("Tamamlandı"),
                                    m_activeFileTotal, m_activeFileTotal, 100.0);
                Q_EMIT progress(m_bytesTotal, m_bytesTotal, 0.0, 0);
                Q_EMIT hashReady(checksumsPath, manifestSha256);
                Q_EMIT finished(true, finalPath, manifestSha256);
                Q_EMIT logMessage(QStringLiteral("[SUCCESS] Çevrimdışı Kurulum Paketi %1 ile başarıyla tamamlandı ve doğrulandı: %2").arg(tool, finalPath));
                Q_EMIT logMessage(QStringLiteral("[SUCCESS] Paket Bütünlük Özeti (SHA-256): %1").arg(manifestSha256));
                Q_EMIT logMessage(QStringLiteral("[INFO] Doğrulama dosyaları kaydedildi: checksums.sha256 / package.info"));
            }, Qt::QueuedConnection);
        });

        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
        return;
    }

    // Tekil dosya modu (.ISO / .IMG)
    const qint64 partSize = QFileInfo(m_partPath).size();
    if (m_bytesTotal > 0 && partSize < m_bytesTotal) {
        Q_EMIT logMessage(QStringLiteral("[ERROR] Dosya boyutu (%1 byte) beklenen boyuttan (%2 byte) küçük, doğrulama iptal edildi.")
                          .arg(partSize).arg(m_bytesTotal));
        m_state = State::Failed;
        Q_EMIT stateChanged(m_state);
        Q_EMIT finished(false, QString(), QStringLiteral("İndirilen dosya eksik (boyut uyuşmuyor)."));
        return;
    }

    if (QFile::exists(m_finalPath)) {
        QFile::remove(m_finalPath);
    }

    if (!QFile::rename(m_partPath, m_finalPath)) {
        m_state = State::Failed;
        Q_EMIT stateChanged(m_state);
        Q_EMIT finished(false, QString(), QStringLiteral("Geçici dosya yeniden adlandırılamadı."));
        return;
    }

    QFile::remove(m_partPath + QStringLiteral(".aria2"));
    QFile::remove(m_partPath + QStringLiteral(".etag"));

    m_state = State::Verifying;
    Q_EMIT stateChanged(m_state);
    Q_EMIT logMessage(QStringLiteral("[INFO] İndirme tamamlandı (%1). SHA-256 bütünlüğü doğrulanıyor...").arg(toolName));

    const QString finalPath = m_finalPath;
    const QString fileName = m_fileName;
    const QString url = m_url;
    const QString tool = toolName;
    const qint64 totalBytes = m_bytesTotal > 0 ? m_bytesTotal : (QFileInfo::exists(finalPath) ? QFileInfo(finalPath).size() : 0);

    QThread *worker = QThread::create([this, finalPath, fileName, url, tool, totalBytes]() {
        QString sha256Hex;
        QFile f(finalPath);
        if (f.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            QByteArray buf(512 * 1024, Qt::Uninitialized);
            while (!f.atEnd()) {
                const qint64 rd = f.read(buf.data(), buf.size());
                if (rd > 0) hash.addData(QByteArrayView(buf.constData(), rd));
            }
            f.close();
            sha256Hex = QString::fromLatin1(hash.result().toHex().toLower());
        }

        if (!sha256Hex.isEmpty()) {
            const QString shaFilePath = finalPath + QStringLiteral(".sha256");
            QFile shaFile(shaFilePath);
            if (shaFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&shaFile);
                ts << sha256Hex << " *" << QFileInfo(finalPath).fileName() << "\n";
                shaFile.close();
            }

            const QString infoFilePath = finalPath + QStringLiteral(".info");
            QFile infoFile(infoFilePath);
            if (infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&infoFile);
                ts << "[Arbitrium Office Download Info]\n";
                ts << "FileName = " << QFileInfo(finalPath).fileName() << "\n";
                ts << "FileSize = " << totalBytes << "\n";
                const double gb = double(totalBytes) / (1024.0 * 1024.0 * 1024.0);
                ts << "FileSizeFormatted = " << QStringLiteral("%1 GB").arg(gb, 0, 'f', 2) << "\n";
                ts << "SHA256 = " << sha256Hex << "\n";
                ts << "DownloadTool = " << tool << "\n";
                ts << "DownloadDate = " << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
                ts << "SourceURL = " << url << "\n";
                ts << "Status = Verified\n";
                infoFile.close();
            }
        }

        QMetaObject::invokeMethod(this, [this, finalPath, tool, sha256Hex]() {
            m_lastSha256 = sha256Hex;
            m_state = State::Completed;
            Q_EMIT stateChanged(m_state);
            Q_EMIT progress(m_bytesTotal, m_bytesTotal, 0.0, 0);
            Q_EMIT hashReady(finalPath, sha256Hex);
            Q_EMIT finished(true, finalPath, sha256Hex);
            Q_EMIT logMessage(QStringLiteral("[SUCCESS] Dosya %1 ile başarıyla indirildi: %2").arg(tool, finalPath));
            if (!sha256Hex.isEmpty()) {
                Q_EMIT logMessage(QStringLiteral("[SUCCESS] SHA-256: %1").arg(sha256Hex));
                Q_EMIT logMessage(QStringLiteral("[INFO] Doğrulama dosyaları kaydedildi: %1.sha256 / %1.info").arg(QFileInfo(finalPath).fileName()));
            }
        }, Qt::QueuedConnection);
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void OfficeDownloader::onAriaFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_state != State::Downloading)
        return;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        if (m_isPackageMode) {
            if (!m_remainingPackageFiles.isEmpty()) {
                const QString rel = m_remainingPackageFiles.first();
                const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
                const QString full = QDir(dataDir).filePath(rel);
                if (isPackageFileComplete(rel, full)) {
                    verifyAndAdvancePackageFile(rel);
                    return;
                } else {
                    Q_EMIT logMessage(QStringLiteral("[WARN] %1 dosyası eksik veya tamamlanmamış, tekrar deneniyor...").arg(rel));
                    downloadNextPackageFile();
                    return;
                }
            } else {
                finalizeDownload(QStringLiteral("Aria2"));
                return;
            }
        } else {
            const qint64 partSize = QFileInfo(m_partPath).size();
            if (m_bytesTotal > 0 && partSize < m_bytesTotal) {
                Q_EMIT logMessage(QStringLiteral("[WARN] Aria2 süreci sonlandı ancak dosya boyutu (%1 byte) beklenen boyuttan (%2 byte) küçük. Devam ettiriliyor...")
                                  .arg(partSize).arg(m_bytesTotal));
                startAria2();
                return;
            }
        }
        finalizeDownload(QStringLiteral("Aria2"));
    } else if (m_state == State::Downloading) {
        const QString errStr = m_ariaProcess ? QString::fromUtf8(m_ariaProcess->readAllStandardError()) : QString();
        m_state = State::Failed;
        Q_EMIT stateChanged(m_state);
        Q_EMIT finished(false, QString(), errStr.isEmpty() ? QStringLiteral("Aria2 indirme hatası (Kod: %1)").arg(exitCode) : errStr);
    }
}

void OfficeDownloader::startWget()
{
    const QString wgetExe = ensureWgetExtracted();
    if (!QFileInfo::exists(wgetExe)) {
        Q_EMIT logMessage(QStringLiteral("[WARN] wget.exe çıkarılamadı, dahili indiriciye dönülüyor..."));
        m_tool = QStringLiteral("internal");
        startInternalRequest(0);
        return;
    }

    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->deleteLater();
    }

    m_ariaProcess = new QProcess(this);
    connect(m_ariaProcess, &QProcess::readyReadStandardOutput, this, &OfficeDownloader::onWgetOutput);
    connect(m_ariaProcess, &QProcess::readyReadStandardError, this, &OfficeDownloader::onWgetOutput);
    connect(m_ariaProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OfficeDownloader::onWgetFinished);

    const QStringList args = {
        QStringLiteral("-O"), m_partPath,
        QStringLiteral("--no-check-certificate"),
        QStringLiteral("-c"),
        QStringLiteral("--no-cache"),
        QStringLiteral("--timeout=15"),
        QStringLiteral("--waitretry=2"),
        QStringLiteral("--progress=bar:force:noscroll"),
        QStringLiteral("--tries=5"),
        QStringLiteral("--user-agent=Arbitrium/1.0 (Windows NT; x64)"),
        m_url
    };

    m_state = State::Downloading;
    Q_EMIT stateChanged(m_state);
    Q_EMIT logMessage(QStringLiteral("[INFO] Wget ile indirme başlatıldı: %1").arg(m_fileName));

    m_speedTimer.start(500);
    m_ariaProcess->start(wgetExe, args);
}

void OfficeDownloader::onWgetOutput()
{
    if (!m_ariaProcess)
        return;

    const QByteArray rawData = m_ariaProcess->readAllStandardOutput() + m_ariaProcess->readAllStandardError();
    const QString text = QString::fromUtf8(rawData);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);

    static const QRegularExpression progressRegex(
        QStringLiteral(R"((\d+)%\s*\[.*?\]\s*([0-9.,]+[KMG]?B?)\s+([0-9.,]+[KMG]?B?/s)?(?:\s+(?:in|eta)\s+([0-9a-zA-Z\s.]+))?)"));
    static const QRegularExpression lengthRegex(QStringLiteral(R"(Length:\s*(\d+))"));
    static const QRegularExpression savingRegex(QStringLiteral(R"(Saving to:\s*['"]?([^'"]+)['"]?)"));

    for (const QString &line : lines) {
        const auto lenMatch = lengthRegex.match(line);
        if (lenMatch.hasMatch()) {
            const qint64 len = lenMatch.captured(1).toLongLong();
            if (len > 0) {
                if (m_isPackageMode) {
                    m_activeFileTotal = len;
                } else if (m_bytesTotal <= 0 || len > m_bytesTotal) {
                    m_bytesTotal = len;
                }
            }
        }

        const auto savingMatch = savingRegex.match(line);
        if (savingMatch.hasMatch()) {
            const QString saved = QFileInfo(savingMatch.captured(1).trimmed()).fileName();
            if (!saved.isEmpty()) {
                m_currentFileName = saved;
            }
        }

        if (m_isPackageMode) {
            if (line.contains(QLatin1String("saved [")) || line.contains(QLatin1String("already fully retrieved"))) {
                updatePackageProgressFromDisk();
                Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                                    m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
            }
        }

        const auto match = progressRegex.match(line);
        if (match.hasMatch()) {
            const double pct = match.captured(1).toDouble();
            const qint64 curBytes = parseUnitBytes(match.captured(2));
            const double speedMBps = match.captured(3).isEmpty() ? 0.0 : (parseUnitBytes(match.captured(3)) / 1048576.0);
            const int etaSec = match.captured(4).isEmpty() ? 0 : parseEtaSeconds(match.captured(4));

            if (speedMBps > 0.01) {
                m_speedMBps = speedMBps;
            }
            if (etaSec > 0) {
                m_etaSeconds = etaSec;
            }

            if (m_isPackageMode) {
                if (curBytes > m_activeFileReceived) {
                    m_activeFileReceived = curBytes;
                }
                m_activeFilePct = pct;
                updatePackageProgressFromDisk();
                Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                                    m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
                Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);
            } else {
                if (curBytes > m_bytesReceived) {
                    m_bytesReceived = curBytes;
                }
                if (m_bytesTotal <= 0 && pct > 0.0 && curBytes > 0) {
                    m_bytesTotal = qint64(double(curBytes) / (pct / 100.0));
                }
                if (m_bytesReceived > m_bytesTotal && m_bytesTotal > 0) {
                    m_bytesTotal = m_bytesReceived;
                }
                Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);
            }
        }
    }
}

void OfficeDownloader::onWgetFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_state != State::Downloading)
        return;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        if (m_isPackageMode) {
            if (!m_remainingPackageFiles.isEmpty()) {
                const QString rel = m_remainingPackageFiles.first();
                const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
                const QString full = QDir(dataDir).filePath(rel);
                if (isPackageFileComplete(rel, full)) {
                    verifyAndAdvancePackageFile(rel);
                    return;
                } else {
                    Q_EMIT logMessage(QStringLiteral("[WARN] %1 dosyası eksik veya tamamlanmamış, tekrar deneniyor...").arg(rel));
                    downloadNextPackageFile();
                    return;
                }
            } else {
                finalizeDownload(QStringLiteral("Wget"));
                return;
            }
        } else {
            const qint64 partSize = QFileInfo(m_partPath).size();
            if (m_bytesTotal > 0 && partSize < m_bytesTotal) {
                Q_EMIT logMessage(QStringLiteral("[WARN] Wget süreci sonlandı ancak dosya boyutu (%1 byte) beklenen boyuttan (%2 byte) küçük. Devam ettiriliyor...")
                                  .arg(partSize).arg(m_bytesTotal));
                startWget();
                return;
            }
        }
        finalizeDownload(QStringLiteral("Wget"));
    } else if (m_state == State::Downloading) {
        const QString errStr = m_ariaProcess ? QString::fromUtf8(m_ariaProcess->readAllStandardError()) : QString();
        m_state = State::Failed;
        Q_EMIT stateChanged(m_state);
        Q_EMIT finished(false, QString(), errStr.isEmpty() ? QStringLiteral("Wget indirme hatası (Kod: %1)").arg(exitCode) : errStr);
    }
}

void OfficeDownloader::startCurl()
{
    const QString curlExe = ensureCurl();
    if (curlExe.isEmpty() || !QFileInfo::exists(curlExe)) {
        Q_EMIT logMessage(QStringLiteral("[WARN] curl.exe bulunamadı, dahili indiriciye dönülüyor..."));
        m_tool = QStringLiteral("internal");
        startInternalRequest(0);
        return;
    }

    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->deleteLater();
    }

    m_ariaProcess = new QProcess(this);
    connect(m_ariaProcess, &QProcess::readyReadStandardOutput, this, &OfficeDownloader::onCurlOutput);
    connect(m_ariaProcess, &QProcess::readyReadStandardError, this, &OfficeDownloader::onCurlOutput);
    connect(m_ariaProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OfficeDownloader::onCurlFinished);

    const QStringList args = {
        QStringLiteral("-q"),
        QStringLiteral("-L"),
        QStringLiteral("-k"),
        QStringLiteral("-C"), QStringLiteral("-"),
        QStringLiteral("--tcp-nodelay"),
        QStringLiteral("--connect-timeout"), QStringLiteral("15"),
        QStringLiteral("--retry"), QStringLiteral("5"),
        QStringLiteral("--retry-delay"), QStringLiteral("2"),
        QStringLiteral("--user-agent"), QStringLiteral("Arbitrium/1.0 (Windows NT; x64)"),
        QStringLiteral("-o"), m_partPath,
        m_url
    };

    m_state = State::Downloading;
    Q_EMIT stateChanged(m_state);
    Q_EMIT logMessage(QStringLiteral("[INFO] cURL ile indirme başlatıldı: %1").arg(m_fileName));

    m_speedTimer.start(500);
    m_ariaProcess->start(curlExe, args);
}

void OfficeDownloader::onCurlOutput()
{
    if (!m_ariaProcess)
        return;

    const QByteArray rawData = m_ariaProcess->readAllStandardOutput() + m_ariaProcess->readAllStandardError();
    const QString text = QString::fromUtf8(rawData);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);

    static const QRegularExpression tableRegex(
        QStringLiteral(R"(^\s*(\d+)\s+([0-9.,]+[A-Za-z]?)\s+(\d+)\s+([0-9.,]+[A-Za-z]?)\s+\d+\s+\d+\s+([0-9.,]+[A-Za-z]?)\s+\d+(?:\s+([0-9:-]+)\s+([0-9:-]+)\s+([0-9:-]+)\s+([0-9.,]+[A-Za-z]?))?)"));
    static const QRegularExpression pctRegex(QStringLiteral(R"((\d+(?:\.\d+)?)%)"));

    for (const QString &line : lines) {
        const auto tableMatch = tableRegex.match(line);
        if (tableMatch.hasMatch()) {
            const double pct = tableMatch.captured(1).toDouble();
            const qint64 total = parseUnitBytes(tableMatch.captured(2));
            const qint64 rec = parseUnitBytes(tableMatch.captured(4));
            // Grup 9 = Current Speed (anlık), Grup 5 = Average DL Speed (ortalama)
            const QString currentSpeedStr = tableMatch.captured(9);
            const double speed = (!currentSpeedStr.isEmpty() ? parseUnitBytes(currentSpeedStr)
                                                             : parseUnitBytes(tableMatch.captured(5))) / 1048576.0;
            const QString timeLeft = tableMatch.captured(8);
            const int eta = parseEtaSeconds(timeLeft);

            if (speed > 0.01) {
                m_speedMBps = speed;
            }
            if (eta > 0) {
                m_etaSeconds = eta;
            }

            if (m_isPackageMode) {
                if (total > 0) {
                    m_activeFileTotal = total;
                }
                m_activeFilePct = pct;
                if (rec > m_activeFileReceived) {
                    m_activeFileReceived = rec;
                }
                updatePackageProgressFromDisk();
                Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                                    m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
            } else {
                // Tekil dosya modunda (.ISO / .IMG)
                const qint64 diskSize = QFileInfo::exists(m_partPath) ? QFileInfo(m_partPath).size() : 0;
                const qint64 cumulativeRec = qMax(diskSize, m_startOffset + rec);
                if (cumulativeRec > m_bytesReceived) {
                    m_bytesReceived = cumulativeRec;
                }

                if (total > 0) {
                    const qint64 fullTotal = m_startOffset + total;
                    if (m_bytesTotal <= 0 || fullTotal > m_bytesTotal) {
                        m_bytesTotal = fullTotal;
                    }
                }
            }
            Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);
            continue;
        }

        const auto pctMatch = pctRegex.match(line);
        if (pctMatch.hasMatch()) {
            const double pct = pctMatch.captured(1).toDouble();
            if (m_isPackageMode) {
                m_activeFilePct = pct;
                updatePackageProgressFromDisk();
                Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                                    m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
            } else {
                const qint64 diskSize = QFileInfo::exists(m_partPath) ? QFileInfo(m_partPath).size() : 0;
                if (diskSize > m_bytesReceived) {
                    m_bytesReceived = diskSize;
                }
            }
            Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);
        }
    }
}

void OfficeDownloader::onCurlFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_state != State::Downloading)
        return;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        if (m_isPackageMode) {
            if (!m_remainingPackageFiles.isEmpty()) {
                const QString rel = m_remainingPackageFiles.first();
                const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
                const QString full = QDir(dataDir).filePath(rel);
                if (isPackageFileComplete(rel, full)) {
                    verifyAndAdvancePackageFile(rel);
                    return;
                } else {
                    Q_EMIT logMessage(QStringLiteral("[WARN] %1 dosyası eksik veya tamamlanmamış, tekrar deneniyor...").arg(rel));
                    downloadNextPackageFile();
                    return;
                }
            } else {
                finalizeDownload(QStringLiteral("cURL"));
                return;
            }
        } else {
            const qint64 partSize = QFileInfo(m_partPath).size();
            if (m_bytesTotal > 0 && partSize < m_bytesTotal) {
                Q_EMIT logMessage(QStringLiteral("[WARN] cURL süreci sonlandı ancak dosya boyutu (%1 byte) beklenen boyuttan (%2 byte) küçük. Devam ettiriliyor...")
                                  .arg(partSize).arg(m_bytesTotal));
                startCurl();
                return;
            }
        }
        finalizeDownload(QStringLiteral("cURL"));
    } else if (m_state == State::Downloading) {
        const QString errStr = m_ariaProcess ? QString::fromUtf8(m_ariaProcess->readAllStandardError()) : QString();
        m_state = State::Failed;
        Q_EMIT stateChanged(m_state);
        Q_EMIT finished(false, QString(), errStr.isEmpty() ? QStringLiteral("cURL indirme hatası (Kod: %1)").arg(exitCode) : errStr);
    }
}

void OfficeDownloader::startInternalRequest(qint64 startOffset)
{
    m_file.setFileName(m_partPath);
    const auto openMode = (startOffset > 0) ? (QIODevice::WriteOnly | QIODevice::Append)
                                            : (QIODevice::WriteOnly | QIODevice::Truncate);

    if (!m_file.open(openMode)) {
        m_state = State::Failed;
        Q_EMIT stateChanged(m_state);
        Q_EMIT finished(false, QString(), QStringLiteral("Dosya açılamadı: %1").arg(m_file.errorString()));
        return;
    }

    QNetworkRequest req((QUrl(m_url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "Arbitrium/1.0 (Windows NT; x64)");

    if (startOffset > 0) {
        req.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(startOffset) + "-");
        if (!m_remoteETag.isEmpty()) {
            req.setRawHeader("If-Range", m_remoteETag.toUtf8());
        }
    }

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &OfficeDownloader::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &OfficeDownloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &OfficeDownloader::onReplyFinished);

    m_lastBytesForSpeed = m_bytesReceived;
    m_elapsedTimer.start();
    m_lastElapsedMs = 0;

    m_state = State::Downloading;
    Q_EMIT stateChanged(m_state);
    m_speedTimer.start(500);

    Q_EMIT logMessage(QStringLiteral("[INFO] Standart indirme başlatıldı: %1").arg(m_fileName));
}

void OfficeDownloader::pause()
{
    if (m_state != State::Downloading)
        return;

    m_state = State::Paused;
    m_speedTimer.stop();
    m_speedMBps = 0.0;
    m_etaSeconds = 0;

    if (m_ariaProcess) {
        m_ariaProcess->disconnect(this);
        m_ariaProcess->kill();
        m_ariaProcess->waitForFinished(2000);
        delete m_ariaProcess;
        m_ariaProcess = nullptr;
    }
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    if (!m_isPackageMode && QFileInfo::exists(m_partPath)) {
        const qint64 actualSize = QFileInfo(m_partPath).size();
        if (actualSize > m_bytesReceived) {
            m_bytesReceived = actualSize;
        }
    }

    Q_EMIT stateChanged(m_state);
    Q_EMIT progress(m_bytesReceived, m_bytesTotal, 0.0, 0);
    Q_EMIT logMessage(QStringLiteral("[INFO] İndirme duraklatıldı."));
}

void OfficeDownloader::resumePackageDownload()
{
    if (m_packageFiles.isEmpty()) {
        // Dosya listesi henüz oluşturulmamış (metadata sorgusu devam ediyor veya iptal edildi)
        return;
    }

    const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
    const QString baseUrl = QStringLiteral("https://officecdn.microsoft.com/db/%1/Office/Data").arg(m_channelGuid);

    if (m_packageFileSizes.isEmpty()) {
        fetchPackageFileSizes(baseUrl, m_packageFiles, [this]() {
            resumePackageDownload();
        });
        return;
    }

    QStringList remainingFiles;

    for (int i = 0; i < m_packageFiles.size(); ++i) {
        const QString rel = m_packageFiles[i];
        const QString full = QDir(dataDir).filePath(rel);

        const bool isComplete = validateOrCleanPackageFile(rel, full);
        if (!isComplete) {
            remainingFiles << rel;
        }
    }

    m_remainingPackageFiles = remainingFiles;
    updatePackageProgressFromDisk();
    m_lastBytesForSpeed = m_bytesReceived;

    Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                        m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
    if (m_bytesReceived > 0) {
        Q_EMIT progress(m_bytesReceived, m_bytesTotal, 0.0, 0);
    }

    if (remainingFiles.isEmpty()) {
        finalizeDownload(m_tool);
        return;
    }

    downloadNextPackageFile();
}

void OfficeDownloader::resume()
{
    if (m_state != State::Paused)
        return;

    m_state = State::Downloading;
    Q_EMIT stateChanged(m_state);
    m_elapsedTimer.start();
    m_lastElapsedMs = 0;

    if (!m_isPackageMode) {
        if (m_tool != QStringLiteral("aria2")) {
            const QString ariaControl = m_partPath + QStringLiteral(".aria2");
            if (QFileInfo::exists(ariaControl)) {
                QFile::remove(ariaControl);
            }
        }
        if (QFileInfo::exists(m_partPath)) {
            const qint64 pSize = QFileInfo(m_partPath).size();
            if (pSize > m_bytesReceived) {
                m_bytesReceived = pSize;
            }
        }
    }
    m_lastBytesForSpeed = m_bytesReceived;
    m_startOffset = m_bytesReceived;

    if (m_isPackageMode) {
        resumePackageDownload();
        return;
    }

    if (m_tool == QStringLiteral("aria2")) {
        startAria2();
    } else if (m_tool == QStringLiteral("wget")) {
        startWget();
    } else if (m_tool == QStringLiteral("curl")) {
        Q_EMIT logMessage(QStringLiteral("[INFO] cURL ile devam ettiriliyor (%1 byte'tan)...").arg(m_startOffset));
        startCurl();
    } else {
        Q_EMIT logMessage(QStringLiteral("[INFO] İndirme devam ettiriliyor (%1 byte'tan)...").arg(m_startOffset));
        startInternalRequest(m_startOffset);
    }
}

void OfficeDownloader::cancel()
{
    m_state = State::Cancelled;
    cleanup();

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Arbitrium");
    QFile::remove(QDir(tempDir).filePath(QStringLiteral("office_wget_list.txt")));
    QFile::remove(QDir(tempDir).filePath(QStringLiteral("office_curl_config.txt")));
    QFile::remove(QDir(tempDir).filePath(QStringLiteral("office_dl_list.txt")));
    m_packageListFile.clear();

    if (m_isPackageMode) {
        if (!m_finalPath.isEmpty() && QDir(m_finalPath).exists()) {
            for (int retry = 0; retry < 5; ++retry) {
                if (QDir(m_finalPath).removeRecursively())
                    break;
                QThread::msleep(100);
            }
        }
    } else {
        if (!m_partPath.isEmpty()) {
            for (int retry = 0; retry < 5; ++retry) {
                if (!QFileInfo::exists(m_partPath) || QFile::remove(m_partPath))
                    break;
                QThread::msleep(100);
            }
            const QString ariaControl = m_partPath + QStringLiteral(".aria2");
            if (QFileInfo::exists(ariaControl)) {
                QFile::remove(ariaControl);
            }
            QFile::remove(m_partPath + QStringLiteral(".etag"));
        }
    }

    m_bytesReceived = 0;
    m_bytesTotal = 0;
    m_speedMBps = 0.0;
    m_etaSeconds = 0;
    m_lastBytesForSpeed = 0;
    m_startOffset = 0;
    m_completedFiles = 0;
    m_packageFileIdx = 0;
    m_activeFileReceived = 0;
    m_activeFileTotal = 0;
    m_activeFilePct = 0.0;
    m_lastSha256.clear();

    Q_EMIT stateChanged(m_state);
    Q_EMIT progress(0, 0, 0.0, 0);
    Q_EMIT logMessage(QStringLiteral("[INFO] İndirme iptal edildi ve geçici dosyalar temizlendi."));
}

void OfficeDownloader::setTool(const QString &tool)
{
    const QString lower = tool.toLower();
    if (m_tool == lower)
        return;

    m_tool = lower;
    Q_EMIT logMessage(QStringLiteral("[INFO] İndirme aracı güncellendi: %1").arg(m_tool));

    if (!m_packageListFile.isEmpty()) {
        QFile::remove(m_packageListFile);
        m_packageListFile.clear();
    }
}

void OfficeDownloader::switchTool(const QString &newTool)
{
    const QString lower = newTool.toLower();
    if (m_tool == lower)
        return;

    if (m_state == State::Downloading) {
        Q_EMIT logMessage(QStringLiteral("[WARN] İndirme devam ederken motor değiştirilemez. Lütfen önce indirmeyi duraklatın."));
        return;
    }

    // Motor geçişi sırasında dosya bütünlüğü denetlenir:
    // Bozuk, boyutu aşmış veya başlığı geçersiz dosyalar temizlenir;
    // geçerli doğrusal baytlar korunarak yeni motorla devam edilir.
    if (m_isPackageMode) {
        const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
        for (const QString &f : m_packageFiles) {
            const QString full = QDir(dataDir).filePath(f);
            validateOrCleanPackageFile(f, full);
        }
        Q_EMIT logMessage(QStringLiteral("[INFO] Motor %1 olarak güncellendi. Devam ettirildiğinde bu motorla indirilecek.").arg(newTool));
    } else {
        if (!m_partPath.isEmpty()) {
            const QString ariaControl = m_partPath + QStringLiteral(".aria2");
            if (QFile::exists(ariaControl)) {
                QFile::remove(ariaControl);
            }
            if (QFile::exists(m_partPath)) {
                const qint64 pSize = QFileInfo(m_partPath).size();
                if (m_bytesTotal > 0 && pSize > m_bytesTotal) {
                    Q_EMIT logMessage(QStringLiteral("[WARN] Diskteki kısmi dosya (%1 byte) sunucu boyutundan (%2 byte) büyük, temizleniyor...")
                                      .arg(pSize).arg(m_bytesTotal));
                    QFile::remove(m_partPath);
                    QFile::remove(m_partPath + QStringLiteral(".etag"));
                    m_bytesReceived = 0;
                    m_startOffset = 0;
                } else if (pSize <= 512) {
                    QFile::remove(m_partPath);
                    QFile::remove(m_partPath + QStringLiteral(".etag"));
                    m_bytesReceived = 0;
                    m_startOffset = 0;
                } else {
                    m_bytesReceived = pSize;
                    m_startOffset = pSize;
                }
            }
            Q_EMIT logMessage(QStringLiteral("[INFO] Motor %1 olarak güncellendi (%2 byte'tan devam edilecek).")
                              .arg(newTool).arg(m_bytesReceived));
        }
    }

    setTool(lower);
}

void OfficeDownloader::onReadyRead()
{
    if (!m_reply || !m_file.isOpen())
        return;

    const QByteArray data = m_reply->readAll();
    if (!data.isEmpty()) {
        m_file.write(data);
    }
}

void OfficeDownloader::onDownloadProgress(qint64 received, qint64 total)
{
    if (m_isPackageMode) {
        m_activeFileReceived = m_packageFileStartOffset + received;
        if (total > 0) {
            m_activeFileTotal = m_packageFileStartOffset + total;
        }
        if (m_activeFileTotal > 0) {
            m_activeFilePct = (double(m_activeFileReceived) / double(m_activeFileTotal)) * 100.0;
        }
    } else {
        m_bytesReceived = m_startOffset + received;
        if (total > 0) {
            m_bytesTotal = m_startOffset + total;
        }
    }
}

void OfficeDownloader::onSpeedTimerTick()
{
    if (m_state != State::Downloading)
        return;

    // 1. Package mode genel ve dosya ilerleme hesaplaması
    if (m_isPackageMode && !m_packageFiles.isEmpty()) {
        updatePackageProgressFromDisk();
    } else if (!m_isPackageMode && !m_partPath.isEmpty()) {
        if (QFileInfo::exists(m_partPath)) {
            const qint64 partSize = QFileInfo(m_partPath).size();
            if (partSize > m_bytesReceived) {
                m_bytesReceived = partSize;
            }
        }
        if (m_bytesReceived > m_bytesTotal && m_bytesTotal > 0) {
            m_bytesTotal = m_bytesReceived;
        }
    }

    // 2. Hız ve Kalan Süre Hesaplama (Her mod için canlı hesaplama)
    if (!m_elapsedTimer.isValid()) {
        m_elapsedTimer.start();
        m_lastElapsedMs = 0;
        m_lastBytesForSpeed = m_bytesReceived;
    }

    const qint64 currentBytes = m_bytesReceived;
    const qint64 deltaBytes = currentBytes - m_lastBytesForSpeed;
    const qint64 currentMs = m_elapsedTimer.elapsed();
    const qint64 deltaMs = currentMs - m_lastElapsedMs;

    if (deltaMs >= 400) {
        if (deltaBytes > 0) {
            const double calculatedSpeed = (qreal(deltaBytes) / 1048576.0) / (qreal(deltaMs) / 1000.0);
            if (calculatedSpeed > 0.01 && calculatedSpeed < 500.0) {
                if (m_speedMBps <= 0.02) {
                    m_speedMBps = calculatedSpeed;
                } else {
                    m_speedMBps = 0.6 * calculatedSpeed + 0.4 * m_speedMBps;
                }
            }
            m_lastBytesForSpeed = currentBytes;
            m_lastElapsedMs = currentMs;
        } else if (deltaMs >= 2000) {
            m_speedMBps *= 0.6;
            if (m_speedMBps < 0.02)
                m_speedMBps = 0.0;
            m_lastElapsedMs = currentMs;
        }

        const qint64 remainingBytes = m_bytesTotal - currentBytes;
        if (m_speedMBps > 0.02 && remainingBytes > 0) {
            m_etaSeconds = int((qreal(remainingBytes) / 1048576.0) / m_speedMBps);
        } else if (remainingBytes <= 0) {
            m_etaSeconds = 0;
        }
    }

    // 3. GUI'ye her yarım saniyede bir güncel değerleri ilet
    Q_EMIT progress(m_bytesReceived, m_bytesTotal, m_speedMBps, m_etaSeconds);

    if (m_isPackageMode) {
        Q_EMIT fileProgress(m_completedFiles, m_totalFiles, m_currentFileName,
                            m_activeFileReceived, m_activeFileTotal, m_activeFilePct);
    }
}

void OfficeDownloader::onReplyFinished()
{
    if (m_state != State::Downloading)
        return;

    m_speedTimer.stop();

    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    const QNetworkReply::NetworkError error = m_reply ? m_reply->error() : QNetworkReply::UnknownNetworkError;
    const QString errorStr = m_reply ? m_reply->errorString() : QStringLiteral("Bilinmeyen ağ hatası");

    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_isPackageMode) {
        if (error == QNetworkReply::NoError) {
            if (!m_remainingPackageFiles.isEmpty()) {
                const QString rel = m_remainingPackageFiles.first();
                const QString dataDir = m_finalPath + QStringLiteral("/Office/Data");
                const QString full = QDir(dataDir).filePath(rel);
                if (isPackageFileComplete(rel, full)) {
                    verifyAndAdvancePackageFile(rel);
                    return;
                } else {
                    Q_EMIT logMessage(QStringLiteral("[WARN] %1 dosyası tam inmemiş, tekrar deneniyor...").arg(rel));
                    m_speedTimer.start(500);
                    downloadNextPackageFile();
                    return;
                }
            } else {
                finalizeDownload(QStringLiteral("Standart"));
                return;
            }
        } else if (error != QNetworkReply::OperationCanceledError) {
            m_state = State::Failed;
            Q_EMIT stateChanged(m_state);
            Q_EMIT finished(false, QString(), errorStr);
            Q_EMIT logMessage(QStringLiteral("[ERROR] Paket indirme hatası: %1").arg(errorStr));
            return;
        }
        return;
    }

    // Tekil dosya modu (.ISO / .IMG)
    if (error == QNetworkReply::NoError) {
        const qint64 partSize = QFileInfo(m_partPath).size();
        if (m_bytesTotal > 0 && partSize < m_bytesTotal) {
            Q_EMIT logMessage(QStringLiteral("[WARN] Yanıt sonlandı ancak dosya boyutu (%1 byte) beklenen boyuttan (%2 byte) küçük. Devam ettiriliyor...")
                              .arg(partSize).arg(m_bytesTotal));
            startInternalRequest(partSize);
            return;
        }
        finalizeDownload(QStringLiteral("Standart"));
    } else if (error != QNetworkReply::OperationCanceledError) {
        m_state = State::Failed;
        Q_EMIT stateChanged(m_state);
        Q_EMIT finished(false, QString(), errorStr);
        Q_EMIT logMessage(QStringLiteral("[ERROR] İndirme başarısız: %1").arg(errorStr));
    }
}
