// preset.h — tweak selections and app preferences saved as XML.
//
// A preset records the position each tweak should sit at, by catalogue id. It deliberately
// does not record registry paths: the catalogue is the single source of truth for where a
// tweak lives, so a preset stays valid when a tweak's definition is corrected, and a preset
// can never be used to smuggle an arbitrary registry write into the app.
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <arbitrium-preset version="3">
//     <meta name="Sessiz kurulum" created="2026-08-15T21:40:12" app="0.9.10" tweaks="37"
//           scope="changed"/>
//     <appearance theme="dark" accent="#e8a33d" typeface="plex" textSize="1" compact="false"
//                 language="tr"/>
//     <settings smoothScroll="true" borderGlow="true" checkUpdates="false"
//               confirmBeforeApply="true"/>
//     <tweaks>
//       <tweak id="priv-07-1310" position="1" name="Reklam kimliği"/>
//       <tweak id="svc-Spooler" position="3" label="Devre dışı" name="Yazdırma Biriktiricisi"/>
//     </tweaks>
//   </arbitrium-preset>
//
// Version 1 wrote on="true|false", which was enough while every tweak was a switch. A
// service has four positions and a choice can have more, so the file carries the index —
// and `label` beside it, for the human reading the file, since an index alone says
// nothing. Version 1 files still load: on="true" is position 1.
//
// Version 3 adds the two elements above and `scope`, and every one of them is optional.
// That is the same bargain version 2 struck with version 1: a reader decides what a file
// carries by looking at what is in it, not by refusing anything whose version number it
// does not recognise. So a version 1 or version 2 file loads exactly as it always did —
// no <appearance>, no <settings>, and `scope` absent means "all", because that is what
// those writers wrote.
//
// Two elements rather than one because they are answerable to different owners: Theme and
// Locale hold the appearance, Settings holds the four switches, and a file that spelled
// them as one block would have to be re-cut the first time one of them moves.

#pragma once

#include <QColor>
#include <QDateTime>
#include <QHash>
#include <QString>

namespace Preset {

/// What the <tweaks> element claims to hold.
///
/// `Changed` is what the settings page writes: the tweaks that are not sitting where
/// Windows ships them, plus anything queued and waiting for Uygula. Not "differs from what
/// is applied" on its own — that is the pending set, and the pending set is empty the
/// moment the user presses Uygula, so a preset exported from a machine the user had just
/// finished configuring would have held nothing at all.
///
/// `All` is every id in the catalogue, which is what versions 1 and 2 always wrote — kept
/// rather than removed, because a full snapshot is a reasonable thing to want and a reader
/// has to be able to tell the two kinds apart.
enum class Scope { Changed, All };

struct Meta
{
    QString name;
    QDateTime created;
    QString appVersion;
    int tweakCount = 0;
    Scope scope = Scope::All;   ///< no scope attribute means a v1/v2 file, which held everything
};

/// The <appearance> element: everything the user chose about how the app looks, plus the
/// interface language, which lives here rather than in <settings> because the settings page
/// draws it among the appearance rows and the user thinks of it as part of the same choice.
///
/// The spellings are the ones the command line already accepts — `theme` takes what
/// `--theme` takes, `typeface` what `--typeface` takes — so a preset and a flag can never
/// disagree about what "dark" or "plex" means.
struct AppearanceBlock
{
    bool present = false;   ///< false when the file carried no <appearance> element at all

    QString theme;          ///< "dark", "light", "midnight", … — Theme's own scheme names
    QColor accent;
    QString typeface;       ///< "plex", "monda", … — an id from Theme::typefaces()
    qreal textSize = 1.0;   ///< Theme::fontScale(), the multiplier over every text size
    bool compact = false;
    QString language;       ///< "tr", "en", … — an id from Locale::languages()
};

/// The <settings> element: the four switches on the Ayarlar page that are not appearance.
struct SettingsBlock
{
    bool present = false;   ///< false when the file carried no <settings> element at all

    bool smoothScroll = true;
    bool borderGlow = true;
    bool checkUpdates = false;
    bool confirmBeforeApply = true;
};

/// The app's own preferences as they stand right now, for writing into a preset. Both come
/// back with present = true, so handing them straight to save() writes both elements.
AppearanceBlock currentAppearance();
SettingsBlock currentSettings();

/// Pushes a loaded block back into Theme, Locale and Settings. A block with present = false
/// is ignored, so an importer can call these unconditionally.
///
/// Unlike a tweak position, which is only queued until the user presses Uygula, these take
/// effect the moment they are applied, and they are persisted. That is not an exception to
/// the app's central promise: nothing here is a machine write. It is the app's own
/// preferences, stored where every other preference is stored — under
/// HKCU\Software\Arbitrium — and importing them is the user asking for this machine's copy
/// of the app to look like the other one from now on. Because it is immediate and visible,
/// the caller is expected to have asked first.
void applyAppearance(const AppearanceBlock &block);
void applySettings(const SettingsBlock &block);

/// Writes the position of every id in \a positions, plus whichever of the two blocks has
/// present set. False and \a error on failure.
bool save(const QString &path, const QString &name, const QHash<QString, int> &positions,
          Scope scope, const AppearanceBlock &appearance, const SettingsBlock &settings,
          QString *error = nullptr);

/// Tweaks only, written as scope="all": a caller that hands over a map of positions and
/// nothing else is describing a whole configuration, not a set of changes. This is the
/// shape --self-test round-trips.
bool save(const QString &path, const QString &name,
          const QHash<QString, int> &positions, QString *error = nullptr);

struct LoadResult
{
    bool ok = false;
    Meta meta;
    QHash<QString, int> positions;
    AppearanceBlock appearance;   ///< present only when the file carried the element
    SettingsBlock settings;       ///< likewise
    int unknownIds = 0;     ///< entries whose id is not in the catalogue any more
    int outOfRange = 0;     ///< positions a tweak no longer offers, clamped on load
    QString error;
};

/// Reads a preset. Ids the catalogue does not know are counted and skipped rather than
/// silently applied.
///
/// The same rule holds for the three named ids inside <appearance>. A theme, typeface or
/// language spelling this build does not know — a file written by a later version, most
/// obviously — is dropped, and the block keeps the value the app is already wearing.
/// Without that, an unknown name would have been worse than an absent one: Theme answers
/// "dark" for any scheme it does not recognise and "plex" for any face it does not
/// recognise, so a preset from a newer build would have quietly repainted the user.
LoadResult load(const QString &path);

/// Writes the pending changes as a .reg file — the format regedit and every other tool on
/// Windows already understands. A preset needs this app and its catalogue to mean
/// anything; a .reg file is the same changes in a form that outlives both.
///
/// \a ids are catalogue ids and \a positions says which option each should end up at.
/// Returns the number of values written, or -1 on failure.
int exportRegFile(const QString &path, const QStringList &ids,
                  const QHash<QString, int> &positions, QString *error = nullptr);

/// Default folder for presets: %APPDATA%/Arbitrium/presets. Created on demand.
QString directory();

/// Suggests a file name for \a name that is safe on Windows.
QString fileNameFor(const QString &name);

} // namespace Preset
