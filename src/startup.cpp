#include "startup.h"
#include "i18n.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace Startup {
namespace {

const QString RunPath = QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString Run32Path = QStringLiteral("Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString ApprovedBase =
    QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\");

QSettings openKey(const QString &hive, const QString &path)
{
    return QSettings(QStringLiteral("HKEY_") + (hive == QLatin1String("HKCU")
                                                    ? QStringLiteral("CURRENT_USER")
                                                    : QStringLiteral("LOCAL_MACHINE"))
                         + QLatin1Char('\\') + path,
                     QSettings::NativeFormat);
}

/// Binary data the way the catalogue and Registry::write() spell it: "02,00,00,…".
QString toHex(const QByteArray &bytes)
{
    QStringList parts;
    parts.reserve(bytes.size());
    for (uchar b : bytes)
        parts << QStringLiteral("%1").arg(b, 2, 16, QLatin1Char('0')).toUpper();
    return parts.join(QLatin1Char(','));
}

void collectRun(const QString &hive, const QString &runPath, const QString &approvedKey,
                const QString &source, QVector<Entry> *out)
{
    QSettings run = openKey(hive, runPath);
    QSettings approved = openKey(hive, ApprovedBase + approvedKey);

    const QStringList names = run.childKeys();
    for (const QString &name : names) {
        Entry entry;
        entry.name = name;
        entry.command = run.value(name).toString();
        entry.source = source;
        entry.approvedHive = hive;
        entry.approvedPath = ApprovedBase + approvedKey;
        entry.approvedValue = name;

        const QByteArray blob = approved.value(name).toByteArray();
        if (!blob.isEmpty()) {
            entry.currentBlob = toHex(blob);
            // Anything with bit 0 set is one of the disabled states Windows writes.
            entry.enabled = (uchar(blob.at(0)) & 1) == 0;
        }

        out->append(entry);
    }
}

void collectFolder(const QString &hive, const QString &folder, const QString &source,
                   QVector<Entry> *out)
{
    QSettings approved = openKey(hive, ApprovedBase + QStringLiteral("StartupFolder"));

    const QFileInfoList files = QDir(folder).entryInfoList(
        {QStringLiteral("*.lnk"), QStringLiteral("*.exe"), QStringLiteral("*.bat"),
         QStringLiteral("*.cmd")},
        QDir::Files);

    for (const QFileInfo &file : files) {
        Entry entry;
        entry.name = file.completeBaseName();
        entry.command = QDir::toNativeSeparators(file.absoluteFilePath());
        entry.source = source;
        entry.approvedHive = hive;
        entry.approvedPath = ApprovedBase + QStringLiteral("StartupFolder");
        entry.approvedValue = file.fileName();

        const QByteArray blob = approved.value(file.fileName()).toByteArray();
        if (!blob.isEmpty()) {
            entry.currentBlob = toHex(blob);
            entry.enabled = (uchar(blob.at(0)) & 1) == 0;
        }

        out->append(entry);
    }
}

} // namespace

QString enabledBlob()
{
    return QStringLiteral("02,00,00,00,00,00,00,00,00,00,00,00");
}

QString disabledBlob()
{
    // Windows stamps the disable time into the last eight bytes; zeroes are accepted and
    // read back as "disabled, time unknown".
    return QStringLiteral("03,00,00,00,00,00,00,00,00,00,00,00");
}

QVector<Entry> enumerate()
{
    QVector<Entry> entries;

#ifdef Q_OS_WIN
    collectRun(QStringLiteral("HKCU"), RunPath, QStringLiteral("Run"),
               QStringLiteral("HKCU"), &entries);
    collectRun(QStringLiteral("HKLM"), RunPath, QStringLiteral("Run"),
               QStringLiteral("HKLM"), &entries);
    collectRun(QStringLiteral("HKLM"), Run32Path, QStringLiteral("Run32"),
               QStringLiteral("HKLM · 32 bit"), &entries);

    const QString userStartup =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)
        + QStringLiteral("/Startup");
    collectFolder(QStringLiteral("HKCU"), userStartup,
                  Locale::tr(QStringLiteral("startup.baslangicKlasoru")), &entries);

    const QString commonStartup = qEnvironmentVariable("ProgramData")
                                  + QStringLiteral("/Microsoft/Windows/Start Menu/Programs/StartUp");
    collectFolder(QStringLiteral("HKLM"), commonStartup,
                  Locale::tr(QStringLiteral("startup.baslangicKlasoruTumKullanicilar")), &entries);
#endif

    return entries;
}

} // namespace Startup
