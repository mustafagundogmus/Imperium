// registry.h — the only place in the app that touches the Windows registry.
//
// Deliberately small: read one value, write one value, delete one value, and the two
// questions that go with them — does this key exist, and am I elevated. Everything the
// tweak engine needs, and nothing it does not. Data is carried as text in the same
// notation the catalogue uses (decimal for DWORD/QWORD, plain text for strings,
// comma-separated hex for binary) so a tweak definition can be read and audited without
// running the app.

#pragma once

#include <QSettings>
#include <QString>

namespace Registry {

enum class Hive { HKLM, HKCU, HKCR, HKU, HKCC, Invalid };

Hive hiveFromString(const QString &name);

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
/// Only ever called where the catalogue itself expressed a position as the key's absence
/// — the `DELETE_KEY` sentinel above — because that is the only place the whole subtree
/// is genuinely the thing being switched. Undoing a write never comes here; see
/// removeEmptyKey().
bool removeKey(Hive hive, const QString &path, QString *error = nullptr);

/// Deletes \a path only if it holds nothing at all: no values, no subkeys.
///
/// This is what undoing a write wants. "The key was not there before I wrote" is a
/// snapshot from one moment, and keys are shared — thirteen catalogue tweaks write into
/// `SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate` alone, and Windows itself, a
/// domain policy or another tool may have written there since. Taking the key away
/// because we happened to create it would take all of that with it, which is precisely
/// what the Log page's revert used to do.
///
/// Never recursive: it deletes through RegDeleteKeyExW, which refuses a key that has
/// subkeys, so even a race between the emptiness check and the delete cannot widen into
/// a subtree removal. Returns false only when the key exists, is empty and still could
/// not be deleted; a key that is occupied is left alone and reported as success, because
/// an empty policy key left behind changes nothing.
bool removeEmptyKey(Hive hive, const QString &path, QString *error = nullptr);

/// A read-only view of one key, for the bulk reads the info modules do.
///
/// read() above is the precise instrument: one value, its real type, and it never creates
/// anything. This is the blunt one, for a caller that wants a dozen values out of the same
/// key and does not care about their types — it was written out by hand in sysinfo.cpp,
/// deepinfo.cpp and startup.cpp, three times over.
///
/// One hazard, worth knowing exactly once rather than rediscovering: QSettings opens a key
/// through RegCreateKeyEx, so pointing it at a key that is not there *creates* it. This
/// function checks for the key with RegOpenKeyEx first and answers an absent one with an
/// empty store, so a read here never leaves a key behind. Still, where a key's existence is
/// itself the answer — several of Windows' pending-restart flags are exactly that — ask
/// keyExists() directly rather than reading values out of the key.
QSettings openKey(Hive hive, const QString &path);

/// True when this process is running with an elevated token.
bool isElevated();

/// True when writing to \a hive needs an elevated token on this machine.
bool requiresElevation(Hive hive);

/// Binary data in one spelling: two lower-case digits a byte, comma separated, nothing
/// else. The same twelve bytes reach a comparison written half a dozen ways — upper case
/// from one source, single digits from another, stray spaces from a hand-authored
/// catalogue entry — and comparing two blobs as text only means anything once both have
/// been through this. Pure string work, so it is there off Windows too.
QString canonicalBinary(const QString &data);

} // namespace Registry
