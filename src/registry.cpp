#include "registry.h"

#include <QStringList>

#include <string>

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
        return QStringLiteral("yönetici yetkisi gerekiyor");
    if (status == ERROR_FILE_NOT_FOUND)
        return QStringLiteral("anahtar bulunamadı");
    return QStringLiteral("Windows hata kodu %1").arg(status);
}

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

#endif // Q_OS_WIN

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

QString hiveToString(Hive hive)
{
    switch (hive) {
    case Hive::HKLM: return QStringLiteral("HKLM");
    case Hive::HKCU: return QStringLiteral("HKCU");
    case Hive::HKCR: return QStringLiteral("HKCR");
    case Hive::HKU:  return QStringLiteral("HKU");
    case Hive::HKCC: return QStringLiteral("HKCC");
    case Hive::Invalid: break;
    }
    return QStringLiteral("?");
}

bool requiresElevation(Hive hive)
{
    // HKCU is the user's own hive; the machine-wide ones are not writable unelevated.
    return hive != Hive::HKCU;
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
    case REG_DWORD:
        out.type = QStringLiteral("DWORD");
        out.data = QString::number(*reinterpret_cast<const quint32 *>(buffer.constData()));
        break;
    case REG_QWORD:
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
        QString text = QString::fromWCharArray(reinterpret_cast<const wchar_t *>(buffer.constData()), chars);
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
        if (error) *error = QStringLiteral("geçersiz kayıt kovanı");
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
    if (error) *error = QStringLiteral("yalnızca Windows");
    return false;
#endif
}

bool remove(Hive hive, const QString &path, const QString &name, QString *error)
{
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root) {
        if (error) *error = QStringLiteral("geçersiz kayıt kovanı");
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
    if (error) *error = QStringLiteral("yalnızca Windows");
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

bool removeKey(Hive hive, const QString &path, QString *error)
{
#ifdef Q_OS_WIN
    HKEY root = nativeHive(hive);
    if (!root) {
        if (error) *error = QStringLiteral("geçersiz kayıt kovanı");
        return false;
    }

    // Refuse to delete a hive root: a catalogue typo must not be able to ask for it.
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("\\")) {
        if (error) *error = QStringLiteral("kök anahtar silinemez");
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
    if (error) *error = QStringLiteral("yalnızca Windows");
    return false;
#endif
}

} // namespace Registry
