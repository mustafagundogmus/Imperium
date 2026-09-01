#include "registry.h"
#include "i18n.h"

#include <QHash>
#include <QStringList>

#include <string>
#include <vector>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace Registry {
namespace {

#ifdef Q_OS_WIN

HKEY nativeHive(Hive hive)
{
    switch (hive) {
    case Hive::HKLM: return HKEY_LOCAL_MACHINE;
    case Hive::HKCU: return HKEY_CURRENT_USER;
    case Hive::HKCR: return HKEY_CLASSES_ROOT;
    case Hive::HKU:  return HKEY_USERS;
    case Hive::HKCC: return HKEY_CURRENT_CONFIG;
    case Hive::Invalid: break;
    }
    return nullptr;
}

const wchar_t *wide(const QString &s)
{
    return reinterpret_cast<const wchar_t *>(s.utf16());
}

QString describe(LONG status)
{
    if (status == ERROR_ACCESS_DENIED)
        return Locale::tr(QStringLiteral("err.needAdmin"));
    if (status == ERROR_FILE_NOT_FOUND)
        return Locale::tr(QStringLiteral("err.keyNotFound"));
    return Locale::tr(QStringLiteral("err.windowsCode")).arg(status);
}

#endif // Q_OS_WIN

QByteArray parseBinary(const QString &data)
{
    QByteArray bytes;
    const QStringList parts = data.split(QLatin1Char(','), Qt::SkipEmptyParts);
    bytes.reserve(parts.size());
    for (const QString &part : parts)
        bytes.append(char(part.trimmed().toUShort(nullptr, 16)));
    return bytes;
}

QString formatBinary(const QByteArray &bytes)
{
    QStringList parts;
    parts.reserve(bytes.size());
    for (unsigned char b : bytes)
        parts << QStringLiteral("%1").arg(b, 2, 16, QLatin1Char('0'));
    return parts.join(QLatin1Char(','));
}

} // namespace

Hive hiveFromString(const QString &name)
{
    const QString upper = name.trimmed().toUpper();
    if (upper == QLatin1String("HKLM") || upper == QLatin1String("HKEY_LOCAL_MACHINE")) return Hive::HKLM;
    if (upper == QLatin1String("HKCU") || upper == QLatin1String("HKEY_CURRENT_USER")) return Hive::HKCU;
    if (upper == QLatin1String("HKCR") || upper == QLatin1String("HKEY_CLASSES_ROOT")) return Hive::HKCR;
    if (upper == QLatin1String("HKU")  || upper == QLatin1String("HKEY_USERS")) return Hive::HKU;
    if (upper == QLatin1String("HKCC") || upper == QLatin1String("HKEY_CURRENT_CONFIG")) return Hive::HKCC;
    return Hive::Invalid;
}

QSettings openKey(Hive hive, const QString &path)
{
    static const QHash<Hive, QString> roots{
        {Hive::HKLM, QStringLiteral("HKEY_LOCAL_MACHINE")},
        {Hive::HKCU, QStringLiteral("HKEY_CURRENT_USER")},
        {Hive::HKCR, QStringLiteral("HKEY_CLASSES_ROOT")},
        {Hive::HKU,  QStringLiteral("HKEY_USERS")},
        {Hive::HKCC, QStringLiteral("HKEY_CURRENT_CONFIG")},
    };

    // QSettings reaches a registry key through RegCreateKeyEx — Qt's own RegistryKey
    // opens with read_only = false and creates what it cannot open — so pointing it at
    // a key that is not there *makes* the key. The info modules read a dozen keys whose
    // absence is the ordinary state of a machine (SecureBoot\State on a BIOS boot, the
    // DataCollection and System policy keys on an unmanaged PC, the vmms service without
    // Hyper-V), and every one of those "reads" was leaving an empty key behind in HKLM.
    // The key is looked for first with RegOpenKeyEx, which creates nothing, and one that
    // is missing is answered with an empty, file-less store: every value reads as absent
    // and nothing on the machine changes. A resource path that does not exist can never
    // be created or written, which is what makes it safe to hand back.
    if (!keyExists(hive, path))
        return QSettings(QStringLiteral(":/arbitrium/absent-key.ini"), QSettings::IniFormat);

    return QSettings(roots.value(hive) + QLatin1Char('\\') + path, QSettings::NativeFormat);
}

bool isElevated()
{
#ifdef Q_OS_WIN
    // Cached: the answer cannot change without the process being replaced.
    static const bool elevated = [] {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        TOKEN_ELEVATION elevation{};
        DWORD size = sizeof(elevation);
        const bool ok = GetTokenInformation(token, TokenElevation, &elevation,
                                            sizeof(elevation), &size);
        CloseHandle(token);
        return ok && elevation.TokenIsElevated != 0;
    }();
    return elevated;
#else
    return false;
#endif
}

bool requiresElevation(Hive hive)
{
    // HKCU is the user's own hive; the machine-wide ones are not writable unelevated.
    return hive != Hive::HKCU;
}

QString canonicalBinary(const QString &data)
{
    return formatBinary(parseBinary(data));
}

Value read(Hive hive, const QString &path, const QString &name)
{
    Value out;
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root)
        return out;

    HKEY key = nullptr;
    if (RegOpenKeyExW(root, wide(path), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return out;

    DWORD type = 0;
    DWORD size = 0;
    LONG status = RegQueryValueExW(key, wide(name), nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS) {
        RegCloseKey(key);
        return out;
    }

    QByteArray buffer(int(size), Qt::Uninitialized);
    status = RegQueryValueExW(key, wide(name), nullptr, &type,
                              reinterpret_cast<LPBYTE>(buffer.data()), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS)
        return out;

    out.exists = true;
    switch (type) {
    // Nothing stops a value being written with the wrong width for its type, and a
    // REG_DWORD carrying two bytes would be read four bytes past the buffer. A value
    // that does not fit its own type is reported as the bytes it actually holds.
    case REG_DWORD:
        if (size < sizeof(quint32)) {
            out.type = QStringLiteral("BINARY");
            out.data = formatBinary(buffer);
            break;
        }
        out.type = QStringLiteral("DWORD");
        out.data = QString::number(*reinterpret_cast<const quint32 *>(buffer.constData()));
        break;
    case REG_QWORD:
        if (size < sizeof(quint64)) {
            out.type = QStringLiteral("BINARY");
            out.data = formatBinary(buffer);
            break;
        }
        out.type = QStringLiteral("QWORD");
        out.data = QString::number(*reinterpret_cast<const quint64 *>(buffer.constData()));
        break;
    case REG_SZ:
    case REG_EXPAND_SZ:
    case REG_MULTI_SZ: {
        out.type = type == REG_SZ ? QStringLiteral("SZ")
                 : type == REG_EXPAND_SZ ? QStringLiteral("EXPAND_SZ")
                                         : QStringLiteral("MULTI_SZ");
        const int chars = int(size / sizeof(wchar_t));
        // Through fromWCharArray with an explicit length, not up to the first null: a
        // MULTI_SZ is several strings separated by nulls, and stopping at the first one
        // would read back a single element and write that back over the whole list.
        QString text = QString::fromWCharArray(reinterpret_cast<const wchar_t *>(buffer.constData()), chars);
        // Only the terminator goes; the separators between a MULTI_SZ's elements stay,
        // and write() puts the terminator back.
        while (text.endsWith(QChar(u'\0')))
            text.chop(1);
        out.data = text;
        break;
    }
    default:
        out.type = QStringLiteral("BINARY");
        out.data = formatBinary(buffer);
        break;
    }
#else
    Q_UNUSED(hive); Q_UNUSED(path); Q_UNUSED(name);
#endif
    return out;
}

bool write(Hive hive, const QString &path, const QString &name,
           const QString &type, const QString &data, QString *error)
{
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root) {
        if (error) *error = Locale::tr(QStringLiteral("err.badHive"));
        return false;
    }

    HKEY key = nullptr;
    DWORD disposition = 0;
    LONG status = RegCreateKeyExW(root, wide(path), 0, nullptr, REG_OPTION_NON_VOLATILE,
                                  KEY_SET_VALUE, nullptr, &key, &disposition);
    if (status != ERROR_SUCCESS) {
        if (error) *error = describe(status);
        return false;
    }

    const QString upper = type.toUpper();
    if (upper == QLatin1String("DWORD")) {
        const DWORD v = data.toUInt();
        status = RegSetValueExW(key, wide(name), 0, REG_DWORD,
                                reinterpret_cast<const BYTE *>(&v), sizeof(v));
    } else if (upper == QLatin1String("QWORD")) {
        const quint64 v = data.toULongLong();
        status = RegSetValueExW(key, wide(name), 0, REG_QWORD,
                                reinterpret_cast<const BYTE *>(&v), sizeof(v));
    } else if (upper == QLatin1String("BINARY")) {
        const QByteArray bytes = parseBinary(data);
        status = RegSetValueExW(key, wide(name), 0, REG_BINARY,
                                reinterpret_cast<const BYTE *>(bytes.constData()), DWORD(bytes.size()));
    } else if (upper == QLatin1String("MULTI_SZ")) {
        // A MULTI_SZ is several strings, each null-terminated, with a second null closing
        // the list. Writing one as REG_SZ — which is what the fall-through below used to
        // do — changes the value's type behind the caller's back, and reverting a
        // journalled MULTI_SZ that way leaves Windows reading a list as a single string.
        //
        // Built by hand rather than through std::wstring, whose c_str() stops at the
        // first embedded null and would hand over only the first element.
        std::vector<wchar_t> text;
        text.reserve(size_t(data.size()) + 2);
        for (QChar c : data)
            text.push_back(wchar_t(c.unicode()));
        while (!text.empty() && text.back() == L'\0')
            text.pop_back();
        text.push_back(L'\0');   // terminates the last element
        text.push_back(L'\0');   // …and the list
        status = RegSetValueExW(key, wide(name), 0, REG_MULTI_SZ,
                                reinterpret_cast<const BYTE *>(text.data()),
                                DWORD(text.size() * sizeof(wchar_t)));
    } else {
        const DWORD kind = (upper == QLatin1String("EXPAND_SZ")) ? REG_EXPAND_SZ : REG_SZ;
        // Through std::wstring rather than QString::utf16(), which hands back a null
        // pointer for a null string. An empty string is a value a catalogue entry may
        // legitimately ask for — it is how the classic context menu is switched on — and
        // a null pointer with a two-byte length is a crash, not a write.
        const std::wstring text = data.toStdWString();
        const DWORD bytes = DWORD((text.size() + 1) * sizeof(wchar_t));
        status = RegSetValueExW(key, wide(name), 0, kind,
                                reinterpret_cast<const BYTE *>(text.c_str()), bytes);
    }

    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        if (error) *error = describe(status);
        return false;
    }
    return true;
#else
    Q_UNUSED(hive); Q_UNUSED(path); Q_UNUSED(name); Q_UNUSED(type); Q_UNUSED(data);
    if (error) *error = Locale::tr(QStringLiteral("err.windowsOnly"));
    return false;
#endif
}

bool remove(Hive hive, const QString &path, const QString &name, QString *error)
{
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root) {
        if (error) *error = Locale::tr(QStringLiteral("err.badHive"));
        return false;
    }

    HKEY key = nullptr;
    LONG status = RegOpenKeyExW(root, wide(path), 0, KEY_SET_VALUE, &key);
    if (status == ERROR_FILE_NOT_FOUND)
        return true;   // nothing to delete is the state we wanted
    if (status != ERROR_SUCCESS) {
        if (error) *error = describe(status);
        return false;
    }

    status = RegDeleteValueW(key, wide(name));
    RegCloseKey(key);

    if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND)
        return true;
    if (error) *error = describe(status);
    return false;
#else
    Q_UNUSED(hive); Q_UNUSED(path); Q_UNUSED(name);
    if (error) *error = Locale::tr(QStringLiteral("err.windowsOnly"));
    return false;
#endif
}

bool keyExists(Hive hive, const QString &path)
{
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root)
        return false;

    HKEY key = nullptr;
    if (RegOpenKeyExW(root, wide(path), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    RegCloseKey(key);
    return true;
#else
    Q_UNUSED(hive); Q_UNUSED(path);
    return false;
#endif
}

bool removeEmptyKey(Hive hive, const QString &path, QString *error)
{
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root) {
        if (error) *error = Locale::tr(QStringLiteral("err.badHive"));
        return false;
    }

    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("\\")) {
        if (error) *error = Locale::tr(QStringLiteral("err.noRootDelete"));
        return false;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(root, wide(trimmed), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return true;   // already gone, which is the state being asked for

    DWORD subkeys = 0;
    DWORD values = 0;
    const LONG info = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, nullptr,
                                       nullptr, &values, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);

    // Somebody else lives here. Leaving the key is the whole point of this function.
    if (info != ERROR_SUCCESS || subkeys != 0 || values != 0)
        return true;

    // RegDeleteKeyExW, not RegDeleteTreeW: it fails rather than recurses if a subkey
    // appeared between the count above and this call.
    const LONG status = RegDeleteKeyExW(root, wide(trimmed), 0, 0);
    if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND)
        return true;

    if (error) *error = describe(status);
    return false;
#else
    Q_UNUSED(hive); Q_UNUSED(path);
    if (error) *error = Locale::tr(QStringLiteral("err.windowsOnly"));
    return false;
#endif
}

bool removeKey(Hive hive, const QString &path, QString *error)
{
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root) {
        if (error) *error = Locale::tr(QStringLiteral("err.badHive"));
        return false;
    }

    // Refuse to delete a hive root: a catalogue typo must not be able to ask for it.
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("\\")) {
        if (error) *error = Locale::tr(QStringLiteral("err.noRootDelete"));
        return false;
    }

    // RegDeleteTreeW takes the subkey down with everything under it, which is what the
    // catalogue means by DELETE_KEY — the keys it applies to are ones the tweak created.
    const LONG status = RegDeleteTreeW(root, wide(trimmed));
    if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND)
        return true;

    if (error) *error = describe(status);
    return false;
#else
    Q_UNUSED(hive); Q_UNUSED(path);
    if (error) *error = Locale::tr(QStringLiteral("err.windowsOnly"));
    return false;
#endif
}

} // namespace Registry
