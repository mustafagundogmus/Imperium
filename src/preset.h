// preset.h — tweak selections saved as XML.
//
// A preset records the position each tweak should sit at, by catalogue id. It deliberately
// does not record registry paths: the catalogue is the single source of truth for where a
// tweak lives, so a preset stays valid when a tweak's definition is corrected, and a preset
// can never be used to smuggle an arbitrary registry write into the app.
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <arbitrium-preset version="2">
//     <meta name="Sessiz kurulum" created="2026-08-15T21:40:12" app="0.9.4" tweaks="37"/>
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

#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

namespace Preset {

struct Meta
{
    QString name;
    QDateTime created;
    QString appVersion;
    int tweakCount = 0;
};

/// Writes the position of every id in \a positions. False and \a error on failure.
bool save(const QString &path, const QString &name,
          const QHash<QString, int> &positions, QString *error = nullptr);

struct LoadResult
{
    bool ok = false;
    Meta meta;
    QHash<QString, int> positions;
    int unknownIds = 0;     ///< entries whose id is not in the catalogue any more
    int outOfRange = 0;     ///< positions a tweak no longer offers, clamped on load
    QString error;
};

/// Reads a preset. Ids the catalogue does not know are counted and skipped rather than
/// silently applied.
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
