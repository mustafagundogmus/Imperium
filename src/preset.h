// preset.h — tweak selections saved as XML.
//
// A preset records which tweaks should be ON, by catalogue id. It deliberately does not
// record registry paths: the catalogue is the single source of truth for where a tweak
// lives, so a preset stays valid when a tweak's definition is corrected, and a preset
// can never be used to smuggle an arbitrary registry write into the app.
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <arbitrium-preset version="1">
//     <meta name="Sessiz kurulum" created="2026-08-15T21:40:12" app="0.9.2" tweaks="37"/>
//     <tweaks>
//       <tweak id="priv-07-1310" on="true" name="Reklam kimliği"/>
//     </tweaks>
//   </arbitrium-preset>

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

/// Writes every id in \a states. Returns false and fills \a error on failure.
bool save(const QString &path, const QString &name,
          const QHash<QString, bool> &states, QString *error = nullptr);

struct LoadResult
{
    bool ok = false;
    Meta meta;
    QHash<QString, bool> states;
    int unknownIds = 0;     ///< entries whose id is not in the catalogue any more
    QString error;
};

/// Reads a preset. Ids the catalogue does not know are counted and skipped rather than
/// silently applied.
LoadResult load(const QString &path);

/// Default folder for presets: %APPDATA%/Arbitrium/presets. Created on demand.
QString directory();

/// Suggests a file name for \a name that is safe on Windows.
QString fileNameFor(const QString &name);

} // namespace Preset
