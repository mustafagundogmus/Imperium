#include "preset.h"
#include "i18n.h"

#include "catalog.h"
#include "registry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace Preset {

namespace {

/// A .reg file spells a hive out in full.
QString fullHive(const QString &hive)
{
    const QString upper = hive.toUpper();
    if (upper == QLatin1String("HKCU")) return QStringLiteral("HKEY_CURRENT_USER");
    if (upper == QLatin1String("HKLM")) return QStringLiteral("HKEY_LOCAL_MACHINE");
    if (upper == QLatin1String("HKCR")) return QStringLiteral("HKEY_CLASSES_ROOT");
    if (upper == QLatin1String("HKU"))  return QStringLiteral("HKEY_USERS");
    if (upper == QLatin1String("HKCC")) return QStringLiteral("HKEY_CURRENT_CONFIG");
    return upper;
}

QString escaped(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return out;
}

/// The value line for one registry entry at one position, in .reg syntax.
QString valueLine(const RegistryEntry &entry, const QString &data)
{
    const QString name = entry.value.isEmpty() ? QStringLiteral("@")
                                               : QStringLiteral("\"%1\"").arg(escaped(entry.value));

    if (data.compare(Registry::DeleteSentinel, Qt::CaseInsensitive) == 0)
        return name + QStringLiteral("=-");

    const QString type = entry.type.toUpper();
    if (type == QLatin1String("DWORD"))
        return QStringLiteral("%1=dword:%2").arg(name, QString::number(data.toUInt(), 16)
                                                           .rightJustified(8, QLatin1Char('0')));
    if (type == QLatin1String("QWORD")) {
        // regedit has no qword literal; it takes the eight bytes little-endian.
        const quint64 v = data.toULongLong();
        QStringList bytes;
        for (int i = 0; i < 8; ++i)
            bytes << QStringLiteral("%1").arg((v >> (8 * i)) & 0xFF, 2, 16, QLatin1Char('0'));
        return QStringLiteral("%1=hex(b):%2").arg(name, bytes.join(QLatin1Char(',')));
    }
    if (type == QLatin1String("BINARY"))
        return QStringLiteral("%1=hex:%2").arg(name, data.toLower().remove(QLatin1Char(' ')));
    if (type == QLatin1String("EXPAND_SZ") || type == QLatin1String("MULTI_SZ")) {
        // Both are stored as UTF-16 bytes in a .reg file.
        QStringList bytes;
        for (QChar c : data) {
            const ushort u = c.unicode();
            bytes << QStringLiteral("%1").arg(u & 0xFF, 2, 16, QLatin1Char('0'))
                  << QStringLiteral("%1").arg((u >> 8) & 0xFF, 2, 16, QLatin1Char('0'));
        }
        bytes << QStringLiteral("00") << QStringLiteral("00");
        const QString kind = type == QLatin1String("EXPAND_SZ") ? QStringLiteral("hex(2)")
                                                                : QStringLiteral("hex(7)");
        return QStringLiteral("%1=%2:%3").arg(name, kind, bytes.join(QLatin1Char(',')));
    }

    return QStringLiteral("%1=\"%2\"").arg(name, escaped(data));
}

} // namespace

int exportRegFile(const QString &path, const QStringList &ids,
                  const QHash<QString, int> &positions, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return -1;
    }

    const Catalog &catalog = Catalog::instance();

    QString text;
    text += QStringLiteral("Windows Registry Editor Version 5.00\r\n\r\n");
    text += QStringLiteral("; Arbitrium %1 · %2\r\n")
                .arg(QCoreApplication::applicationVersion(),
                     QDateTime::currentDateTime().toString(Qt::ISODate));
    text += QStringLiteral("; %1 tweak\r\n\r\n").arg(ids.size());

    int written = 0;
    QString openKey;   // the [key] header currently in effect
    for (const QString &id : ids) {
        const Tweak *tweak = catalog.tweak(id);
        if (!tweak || tweak->options.isEmpty())
            continue;

        const int position = qBound(0, positions.value(id, 0), int(tweak->options.size()) - 1);
        const TweakOption &option = tweak->options.at(position);

        if (!openKey.isEmpty()) {
            text += QStringLiteral("\r\n");
            openKey.clear();
        }

        text += QStringLiteral("; %1").arg(tweak->name);
        if (!option.displayLabel().isEmpty())
            text += QStringLiteral(" — %1").arg(option.displayLabel());
        text += QStringLiteral("\r\n");

        for (int i = 0; i < tweak->reg.size(); ++i) {
            const RegistryEntry &entry = tweak->reg.at(i);
            const QString data = option.data.value(i);
            const QString key = QStringLiteral("%1\\%2").arg(fullHive(entry.hive), entry.path);

            if (data.compare(Registry::DeleteKeySentinel, Qt::CaseInsensitive) == 0) {
                text += QStringLiteral("[-%1]\r\n\r\n").arg(key);
                openKey.clear();
                ++written;
                continue;
            }

            // One header per key: a tweak that owns several values under the same key
            // writes them as one block, the way a hand-written .reg file would.
            if (key != openKey) {
                text += QStringLiteral("[%1]\r\n").arg(key);
                openKey = key;
            }
            text += valueLine(entry, data) + QStringLiteral("\r\n");
            ++written;
        }
    }

    if (!openKey.isEmpty())
        text += QStringLiteral("\r\n");

    // regedit reads UTF-16LE with a BOM; anything else and Turkish paths come out wrong.
    QByteArray bytes;
    bytes.append('\xFF');
    bytes.append('\xFE');
    for (QChar c : text) {
        const ushort u = c.unicode();
        bytes.append(char(u & 0xFF));
        bytes.append(char((u >> 8) & 0xFF));
    }
    file.write(bytes);
    file.close();

    if (file.error() != QFileDevice::NoError) {
        if (error)
            *error = file.errorString();
        return -1;
    }
    return written;
}

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
          const QHash<QString, int> &positions, QString *error)
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
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("2"));

    xml.writeEmptyElement(QStringLiteral("meta"));
    xml.writeAttribute(QStringLiteral("name"), name);
    xml.writeAttribute(QStringLiteral("created"),
                       QDateTime::currentDateTime().toString(Qt::ISODate));
    xml.writeAttribute(QStringLiteral("app"), QCoreApplication::applicationVersion());
    xml.writeAttribute(QStringLiteral("tweaks"), QString::number(positions.size()));

    xml.writeStartElement(QStringLiteral("tweaks"));
    // Sorted so two saves of the same selection produce identical files.
    QStringList ids = positions.keys();
    ids.sort();
    for (const QString &id : std::as_const(ids)) {
        const int position = positions.value(id);
        xml.writeEmptyElement(QStringLiteral("tweak"));
        xml.writeAttribute(QStringLiteral("id"), id);
        xml.writeAttribute(QStringLiteral("position"), QString::number(position));

        // Carried for humans reading the file; the id and the position are what get
        // applied. A switch has no labels, so it says on/off instead.
        if (const Tweak *t = catalog.tweak(id)) {
            xml.writeAttribute(QStringLiteral("name"), t->name);
            // displayLabel(), not the raw field: a synthesised position (a service's
            // "Devre dışı", a startup entry's "Açık") names itself by key and carries no
            // literal label at all, so reading `label` wrote every one of them as on/off.
            const QString label = t->options.value(position).displayLabel();
            xml.writeAttribute(QStringLiteral("label"),
                               label.isEmpty() ? (position == 0 ? Locale::tr(QStringLiteral("preset.off"))
                                                                : Locale::tr(QStringLiteral("preset.on")))
                                               : label);
        }
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
            const Tweak *tweak = catalog.tweak(id);
            if (!tweak) {
                ++result.unknownIds;   // an older preset, or a tweak that was removed
                continue;
            }
            int position = 0;
            if (a.hasAttribute(QStringLiteral("position"))) {
                position = a.value(QStringLiteral("position")).toInt();
            } else {
                // Version 1: a switch, written as on="true|false".
                position = a.value(QStringLiteral("on")).toString() == QLatin1String("true") ? 1 : 0;
            }

            // A preset written against a catalogue that offered more positions than this
            // one does must not be able to ask for one that no longer exists.
            const int last = qMax(0, int(tweak->options.size()) - 1);
            if (position < 0 || position > last) {
                position = qBound(0, position, last);
                ++result.outOfRange;
            }
            result.positions.insert(id, position);
        }
    }

    if (xml.hasError()) {
        result.error = xml.errorString();
        return result;
    }
    if (!sawRoot) {
        result.error = Locale::tr(QStringLiteral("preset.notAPreset"));
        return result;
    }

    result.meta.tweakCount = int(result.positions.size());
    result.ok = true;
    return result;
}

} // namespace Preset
