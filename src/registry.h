// registry.h — the only place in the app that touches the Windows registry.
//
// Deliberately small: read one value, write one value, delete one value. Everything the
// tweak engine needs, and nothing it does not. Data is carried as text in the same
// notation the catalogue uses (decimal for DWORD/QWORD, plain text for strings,
// comma-separated hex for binary) so a tweak definition can be read and audited without
// running the app.

#pragma once

#include <QString>

namespace Registry {

enum class Hive { HKLM, HKCU, HKCR, HKU, HKCC, Invalid };

Hive hiveFromString(const QString &name);
QString hiveToString(Hive hive);

/// The sentinel a catalogue entry uses when a state is expressed by the value's absence.
inline const QString DeleteSentinel = QStringLiteral("DELETE");

/// …and when it is the whole key that must be absent. A handful of Windows behaviours
/// are switched by a key's existence rather than by anything inside it — the classic
/// context menu is the well-known one — and for those, deleting the value is not enough:
/// the empty key left behind still overrides what it shadows.
inline const QString DeleteKeySentinel = QStringLiteral("DELETE_KEY");

struct Value
{
    bool exists = false;
    QString type;    ///< DWORD | QWORD | SZ | EXPAND_SZ | MULTI_SZ | BINARY | ""
    QString data;
};

/// Reads a single value. A missing key or value comes back as `exists == false`,
/// which is not an error — plenty of tweaks are expressed exactly that way.
Value read(Hive hive, const QString &path, const QString &name);

/// Creates the key if needed and writes the value. Returns false and fills \a error
/// on failure; ACCESS_DENIED is reported as a plain "needs administrator" message.
bool write(Hive hive, const QString &path, const QString &name,
           const QString &type, const QString &data, QString *error = nullptr);

/// Deletes a single value. Succeeds when the value is already absent.
bool remove(Hive hive, const QString &path, const QString &name, QString *error = nullptr);

/// True when the key itself is there, whatever it does or does not contain.
bool keyExists(Hive hive, const QString &path);

/// Deletes a key and everything under it. Succeeds when the key is already absent.
///
/// Only ever called for a key this app created: TweakEngine records whether the key was
/// there before it wrote, and undoes a tweak by deleting the value instead whenever the
/// key predates us. Otherwise turning a tweak off could take unrelated settings with it.
bool removeKey(Hive hive, const QString &path, QString *error = nullptr);

/// True when writing to \a hive needs an elevated token on this machine.
bool requiresElevation(Hive hive);

/// Binary data in one spelling: two lower-case digits a byte, comma separated, nothing
/// else. The same twelve bytes reach a comparison written half a dozen ways — upper case
/// from one source, single digits from another, stray spaces from a hand-authored
/// catalogue entry — and comparing two blobs as text only means anything once both have
/// been through this. Pure string work, so it is there off Windows too.
QString canonicalBinary(const QString &data);

} // namespace Registry
