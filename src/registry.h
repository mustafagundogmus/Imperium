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

/// True when writing to \a hive needs an elevated token on this machine.
bool requiresElevation(Hive hive);

} // namespace Registry
