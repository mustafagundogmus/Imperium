#include "preset.h"
#include "i18n.h"

#include "catalog.h"
#include "registry.h"
#include "settings.h"
#include "theme.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

// Theme keeps the canonical spelling of a colour scheme — the very table `--theme` parses
// and the very string QSettings stores — in theme.cpp, but does not declare these two in
// theme.h. They are declared here rather than reimplemented because the whole point of
// reusing them is that a preset and the command line can never disagree about what "dark"
// or "ocean" means; a private copy of the table would be free to drift from the one Theme
// actually persists, and the drift would show up as a preset that silently loads the wrong
// palette. These two lines belong in theme.h the next time that header is edited.
namespace Theme {
QString schemeToString(Appearance a);
Appearance schemeFromString(const QString &name);
}

namespace Preset {

namespace {

// --------------------------------------------------------------------- XML spellings ---

/// The `scope` attribute. A word rather than a number, for the same reason the theme is
/// stored by name: somebody reading the file should be able to tell what it holds.
QString scopeToString(Scope scope)
{
    return scope == Scope::Changed ? QStringLiteral("changed") : QStringLiteral("all");
}

/// Anything that is not "changed" reads as "all", including the attribute being absent
/// altogether — which is how every version 1 and version 2 file arrives here, and those
/// files did carry every id.
Scope scopeFromString(QStringView name)
{
    return name == QLatin1String("changed") ? Scope::Changed : Scope::All;
}

QString boolToString(bool on)
{
    return on ? QStringLiteral("true") : QStringLiteral("false");
}

/// \a fallback rather than false for anything unrecognised: the caller seeds these blocks
/// with what the app is already wearing, so a malformed attribute leaves that preference
/// alone instead of quietly switching it off.
bool boolFromString(QStringView text, bool fallback)
{
    if (text == QLatin1String("true"))
        return true;
    if (text == QLatin1String("false"))
        return false;
    return fallback;
}

// ------------------------------------------------------------- names Theme knows ---
//
// The three named ids in <appearance> all go through a setter that answers a default for
// anything it does not recognise: schemeFromString returns Dark, faceFor returns the first
// face, and those defaults are exactly right for a QSettings value this build wrote itself
// — an appearance/theme key holding rubbish should land somewhere rather than nowhere.
//
// A preset is not that. It can have been written by a later version carrying a ninth
// palette or a fourth face, and handing that name to the setter would repaint the user
// Dark in Plex to say "I have not heard of this". An unknown name would then be *worse*
// than an absent one, which load() carefully treats as "leave this preference alone". So
// the file's name is checked against what this build actually knows before it is stored,
// and a name that fails leaves the seeded value standing.

/// Round-tripping the name back out is how Theme is asked whether it really knew it: only
/// a scheme it recognises spells itself the same way again. Costs nothing to keep in step
/// when a ninth scheme is added, which a table copied into this file would not.
bool knownScheme(const QString &spelling)
{
    return Theme::schemeToString(Theme::schemeFromString(spelling)) == spelling;
}

bool knownTypeface(const QString &id)
{
    const QVector<Theme::Typeface> &faces = Theme::typefaces();
    return std::any_of(faces.cbegin(), faces.cend(),
                       [&id](const Theme::Typeface &f) { return f.id == id; });
}

/// Locale::setLanguage already ignores an id it does not know, so this changes no
/// behaviour on apply. It is here so LoadResult reports the truth: a caller showing the
/// user which language the file asks for should be shown the one that will actually be
/// set, not a string nothing will ever act on.
bool knownLanguage(const QString &id)
{
    const QVector<Locale::Language> &langs = Locale::languages();
    return std::any_of(langs.cbegin(), langs.cend(),
                       [&id](const Locale::Language &l) { return l.id == id; });
}

// ------------------------------------------------------------------------ .reg file ---

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

    // Keys already emitted as [-key]. In .reg syntax a plain [key] header *creates* the
    // key, so a value line written under a key the file has just deleted put it straight
    // back — and about twenty ctx-* tweaks are shaped exactly that way, with the DELETE_KEY
    // on the first entry and its siblings under the same key after it. The exported file
    // then did not reproduce what Apply does, which is the one thing it is for.
    QStringList deletedKeys;
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
                deletedKeys << key;
                ++written;
                continue;
            }

            // Descendants too: [-key] takes the whole subtree, exactly as
            // Registry::removeKey does, and a later header naming a child would rebuild
            // the child and its parent along with it.
            const bool gone = std::any_of(
                deletedKeys.cbegin(), deletedKeys.cend(), [&key](const QString &d) {
                    return key.compare(d, Qt::CaseInsensitive) == 0
                           || key.startsWith(d + QLatin1Char('\\'), Qt::CaseInsensitive);
                });
            if (gone)
                continue;   // its values went with the key

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

AppearanceBlock currentAppearance()
{
    AppearanceBlock block;
    block.present = true;
    block.theme = Theme::schemeToString(Theme::appearance());
    block.accent = Theme::accent();
    block.typeface = Theme::typeface();
    block.textSize = Theme::fontScale();
    block.compact = Theme::compact();
    block.language = Locale::language();
    return block;
}

SettingsBlock currentSettings()
{
    SettingsBlock block;
    block.present = true;
    block.smoothScroll = ::Settings::instance().smoothScroll();
    block.borderGlow = ::Settings::instance().borderGlow();
    block.checkUpdates = ::Settings::instance().checkUpdatesOnLaunch();
    block.confirmBeforeApply = ::Settings::instance().confirmBeforeApply();
    return block;
}

void applyAppearance(const AppearanceBlock &block)
{
    if (!block.present)
        return;

    // Persist::Yes, which is every setter's default, so it is left unwritten here. The
    // command line passes Persist::No because a flag is a one-shot override for a single
    // run — `--theme light` to take one screenshot must not rewrite the look the user
    // saved. An import is the opposite: the user opened a file and answered a dialog
    // asking whether to adopt these, so the answer has to survive the next launch.
    Theme::setAppearance(Theme::schemeFromString(block.theme));
    if (block.accent.isValid())
        Theme::setAccent(block.accent);
    Theme::setTypeface(block.typeface);
    Theme::setFontScale(block.textSize);
    Theme::setCompact(block.compact);

    // Last of the six, because it is the only one that tears the settings page down and
    // rebuilds it: languageChanged is what SettingsPage::rebuild() listens to. Anything
    // after it would be running while the page that called us is being replaced.
    Locale::setLanguage(block.language);
}

void applySettings(const SettingsBlock &block)
{
    if (!block.present)
        return;
    ::Settings::instance().setSmoothScroll(block.smoothScroll);
    ::Settings::instance().setBorderGlow(block.borderGlow);
    ::Settings::instance().setCheckUpdatesOnLaunch(block.checkUpdates);
    ::Settings::instance().setConfirmBeforeApply(block.confirmBeforeApply);
}

bool save(const QString &path, const QString &name,
          const QHash<QString, int> &positions, QString *error)
{
    // No blocks, and scope="all": a caller that hands over a map of positions and nothing
    // else is describing a whole configuration rather than a set of changes.
    return save(path, name, positions, Scope::All, AppearanceBlock(), SettingsBlock(), error);
}

bool save(const QString &path, const QString &name, const QHash<QString, int> &positions,
          Scope scope, const AppearanceBlock &appearance, const SettingsBlock &settings,
          QString *error)
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
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("3"));

    xml.writeEmptyElement(QStringLiteral("meta"));
    xml.writeAttribute(QStringLiteral("name"), name);
    xml.writeAttribute(QStringLiteral("created"),
                       QDateTime::currentDateTime().toString(Qt::ISODate));
    xml.writeAttribute(QStringLiteral("app"), QCoreApplication::applicationVersion());
    xml.writeAttribute(QStringLiteral("tweaks"), QString::number(positions.size()));
    xml.writeAttribute(QStringLiteral("scope"), scopeToString(scope));

    // Both blocks are skipped entirely when the caller did not supply them, so the file a
    // tweaks-only save writes is byte-for-byte what version 2 wrote apart from its version
    // number and the scope attribute.
    if (appearance.present) {
        xml.writeEmptyElement(QStringLiteral("appearance"));
        xml.writeAttribute(QStringLiteral("theme"), appearance.theme);
        xml.writeAttribute(QStringLiteral("accent"), appearance.accent.name(QColor::HexRgb));
        xml.writeAttribute(QStringLiteral("typeface"), appearance.typeface);
        // 'g' with three significant digits, because the four steps are 0.9, 1.0, 1.15 and
        // 1.3 and the default formatting would spell 1.15 as 1.1499999999999999.
        xml.writeAttribute(QStringLiteral("textSize"),
                           QString::number(appearance.textSize, 'g', 3));
        xml.writeAttribute(QStringLiteral("compact"), boolToString(appearance.compact));
        xml.writeAttribute(QStringLiteral("language"), appearance.language);
    }

    if (settings.present) {
        xml.writeEmptyElement(QStringLiteral("settings"));
        xml.writeAttribute(QStringLiteral("smoothScroll"), boolToString(settings.smoothScroll));
        xml.writeAttribute(QStringLiteral("borderGlow"), boolToString(settings.borderGlow));
        xml.writeAttribute(QStringLiteral("checkUpdates"), boolToString(settings.checkUpdates));
        xml.writeAttribute(QStringLiteral("confirmBeforeApply"),
                           boolToString(settings.confirmBeforeApply));
    }

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

    // Seeded from what the app is wearing right now, then cleared of `present`. An
    // attribute a file leaves out therefore means "leave this one alone" rather than
    // "reset it to whatever default this reader happens to carry", and a version 1 or
    // version 2 file — which has neither element, so never sets `present` — cannot move a
    // single preference no matter what the rest of it says.
    result.appearance = currentAppearance();
    result.appearance.present = false;
    result.settings = currentSettings();
    result.settings.present = false;

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
            result.meta.scope = scopeFromString(a.value(QStringLiteral("scope")));
        } else if (name == QLatin1String("appearance")) {
            // Version 3, and optional — which is why nothing here consults the version
            // attribute. The tweak reader below decides what a file carries by looking for
            // `position` and falling back to `on`, not by refusing anything that does not
            // say version 2; these two elements work the same way, so a file with neither
            // is an ordinary file rather than an error.
            const QXmlStreamAttributes a = xml.attributes();
            result.appearance.present = true;
            // knownScheme, not a bare assignment: a name this build has never heard of is
            // dropped so the seeded value stands — see the note above knownScheme().
            const QString scheme = a.value(QStringLiteral("theme")).toString();
            if (knownScheme(scheme))
                result.appearance.theme = scheme;
            if (a.hasAttribute(QStringLiteral("accent"))) {
                // Dropped rather than stored when it does not parse: Theme::setAccent
                // refuses an invalid colour anyway, and keeping the seeded one means the
                // user's own accent survives a mangled file.
                const QColor c(a.value(QStringLiteral("accent")).toString());
                if (c.isValid())
                    result.appearance.accent = c;
            }
            const QString face = a.value(QStringLiteral("typeface")).toString();
            if (knownTypeface(face))
                result.appearance.typeface = face;
            if (a.hasAttribute(QStringLiteral("textSize"))) {
                // Theme::setFontScale clamps to [0.85, 1.6] itself, so a file asking for
                // 40 lands on the largest step rather than being rejected.
                const double size = a.value(QStringLiteral("textSize")).toDouble();
                if (size > 0.0)
                    result.appearance.textSize = size;
            }
            result.appearance.compact = boolFromString(a.value(QStringLiteral("compact")),
                                                       result.appearance.compact);
            const QString lang = a.value(QStringLiteral("language")).toString();
            if (knownLanguage(lang))
                result.appearance.language = lang;
        } else if (name == QLatin1String("settings")) {
            const QXmlStreamAttributes a = xml.attributes();
            SettingsBlock &s = result.settings;
            s.present = true;
            s.smoothScroll = boolFromString(a.value(QStringLiteral("smoothScroll")), s.smoothScroll);
            s.borderGlow = boolFromString(a.value(QStringLiteral("borderGlow")), s.borderGlow);
            s.checkUpdates = boolFromString(a.value(QStringLiteral("checkUpdates")), s.checkUpdates);
            s.confirmBeforeApply = boolFromString(a.value(QStringLiteral("confirmBeforeApply")),
                                                  s.confirmBeforeApply);
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
