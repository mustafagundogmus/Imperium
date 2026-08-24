#include "startup.h"
#include "registry.h"

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

/// The approval blob for one entry, in the spelling the catalogue and the tweak engine
/// both use.
///
/// Read through Registry rather than through the QSettings above, because QSettings
/// decodes a REG_BINARY as UTF-16 text: twelve bytes come back as six, and any byte pair
/// that is not a plain ASCII character comes back as something else entirely. These
/// blobs are twelve bytes with a FILETIME in the tail, which is exactly the shape that
/// destroys — and a blob that reads back wrong never matches the registry again, so
/// every entry the user disabled reappears switched on the next time the app starts.
QString approvedBlob(const QString &hive, const QString &path, const QString &name)
{
    const Registry::Value value = Registry::read(Registry::hiveFromString(hive), path, name);
    return value.exists ? value.data : QString();
}

/// Windows disables an entry by setting bit 0 of the blob's first byte — 2 is enabled,
/// 3 is disabled. No blob at all means nothing has an opinion, and Windows runs it.
bool blobSaysEnabled(const QString &blob)
{
    if (blob.isEmpty())
        return true;
    const QString first = blob.section(QLatin1Char(','), 0, 0).trimmed();
    return (first.toUShort(nullptr, 16) & 1) == 0;
}

void collectRun(const QString &hive, const QString &runPath, const QString &approvedKey,
                const QString &source, QVector<Entry> *out)
{
    QSettings run = openKey(hive, runPath);

    const QStringList names = run.childKeys();
    for (const QString &name : names) {
        Entry entry;
        entry.name = name;
        entry.command = run.value(name).toString();
        entry.source = source;
        entry.approvedHive = hive;
        entry.approvedPath = ApprovedBase + approvedKey;
        entry.approvedValue = name;

        entry.currentBlob = approvedBlob(hive, entry.approvedPath, name);
        entry.enabled = blobSaysEnabled(entry.currentBlob);

        out->append(entry);
    }
}

void collectFolder(const QString &hive, const QString &folder, const QString &sourceKey,
                   QVector<Entry> *out)
{
    const QFileInfoList files = QDir(folder).entryInfoList(
        {QStringLiteral("*.lnk"), QStringLiteral("*.exe"), QStringLiteral("*.bat"),
         QStringLiteral("*.cmd")},
        QDir::Files);

    for (const QFileInfo &file : files) {
        Entry entry;
        entry.name = file.completeBaseName();
        entry.command = QDir::toNativeSeparators(file.absoluteFilePath());
        entry.sourceKey = sourceKey;
        entry.approvedHive = hive;
        entry.approvedPath = ApprovedBase + QStringLiteral("StartupFolder");
        entry.approvedValue = file.fileName();

        entry.currentBlob = approvedBlob(hive, entry.approvedPath, entry.approvedValue);
        entry.enabled = blobSaysEnabled(entry.currentBlob);

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
                  QStringLiteral("startup.baslangicKlasoru"), &entries);

    const QString commonStartup = qEnvironmentVariable("ProgramData")
                                  + QStringLiteral("/Microsoft/Windows/Start Menu/Programs/StartUp");
    collectFolder(QStringLiteral("HKLM"), commonStartup,
                  QStringLiteral("startup.baslangicKlasoruTumKullanicilar"), &entries);
#endif

    return entries;
}

} // namespace Startup
