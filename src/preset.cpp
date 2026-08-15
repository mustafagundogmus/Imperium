#include "preset.h"

#include "catalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace Preset {

QString directory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/presets");
    QDir().mkpath(dir);
    return dir;
}

QString fileNameFor(const QString &name)
{
    QString safe = name.simplified();
    // Windows forbids these outright; spaces become underscores so the name stays one token.
    static const QString illegal = QStringLiteral("<>:\"/\\|?*");
    for (int i = 0; i < safe.size(); ++i) {
        if (illegal.contains(safe.at(i)) || safe.at(i) < QChar(0x20))
            safe[i] = QLatin1Char('-');
    }
    safe.replace(QLatin1Char(' '), QLatin1Char('_'));
    while (safe.endsWith(QLatin1Char('.')))
        safe.chop(1);
    if (safe.isEmpty())
        safe = QStringLiteral("onayar");
    return safe + QStringLiteral(".xml");
}

bool save(const QString &path, const QString &name,
          const QHash<QString, bool> &states, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    const Catalog &catalog = Catalog::instance();

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("arbitrium-preset"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));

    xml.writeEmptyElement(QStringLiteral("meta"));
    xml.writeAttribute(QStringLiteral("name"), name);
    xml.writeAttribute(QStringLiteral("created"),
                       QDateTime::currentDateTime().toString(Qt::ISODate));
    xml.writeAttribute(QStringLiteral("app"), QCoreApplication::applicationVersion());
    xml.writeAttribute(QStringLiteral("tweaks"), QString::number(states.size()));

    xml.writeStartElement(QStringLiteral("tweaks"));
    // Sorted so two saves of the same selection produce identical files.
    QStringList ids = states.keys();
    ids.sort();
    for (const QString &id : std::as_const(ids)) {
        xml.writeEmptyElement(QStringLiteral("tweak"));
        xml.writeAttribute(QStringLiteral("id"), id);
        xml.writeAttribute(QStringLiteral("on"), states.value(id) ? QStringLiteral("true")
                                                                 : QStringLiteral("false"));
        // Carried for humans reading the file; the id is what is actually applied.
        if (const Tweak *t = catalog.tweak(id))
            xml.writeAttribute(QStringLiteral("name"), t->name);
    }
    xml.writeEndElement();   // tweaks

    xml.writeEndElement();   // arbitrium-preset
    xml.writeEndDocument();

    file.close();
    if (file.error() != QFileDevice::NoError) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

LoadResult load(const QString &path)
{
    LoadResult result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }

    const Catalog &catalog = Catalog::instance();
    QXmlStreamReader xml(&file);

    bool sawRoot = false;
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;

        const QStringView name = xml.name();
        if (name == QLatin1String("arbitrium-preset")) {
            sawRoot = true;
        } else if (name == QLatin1String("meta")) {
            const QXmlStreamAttributes a = xml.attributes();
            result.meta.name = a.value(QStringLiteral("name")).toString();
            result.meta.created = QDateTime::fromString(a.value(QStringLiteral("created")).toString(),
                                                        Qt::ISODate);
            result.meta.appVersion = a.value(QStringLiteral("app")).toString();
        } else if (name == QLatin1String("tweak")) {
            const QXmlStreamAttributes a = xml.attributes();
            const QString id = a.value(QStringLiteral("id")).toString();
            if (id.isEmpty())
                continue;
            if (!catalog.tweak(id)) {
                ++result.unknownIds;   // an older preset, or a tweak that was removed
                continue;
            }
            result.states.insert(id, a.value(QStringLiteral("on")).toString() == QLatin1String("true"));
        }
    }

    if (xml.hasError()) {
        result.error = xml.errorString();
        return result;
    }
    if (!sawRoot) {
        result.error = QStringLiteral("bu bir Arbitrium ön ayar dosyası değil");
        return result;
    }

    result.meta.tweakCount = int(result.states.size());
    result.ok = true;
    return result;
}

} // namespace Preset
