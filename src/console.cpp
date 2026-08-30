#include "console.h"

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>

#  include <QVarLengthArray>
#endif

namespace Console {

QString decode(const QByteArray &bytes)
{
#ifdef Q_OS_WIN
    if (bytes.isEmpty())
        return {};

    // The child gets a console of its own, so the code page is the system's console
    // default: HKCU\Console\CodePage when the user has pinned one, otherwise the OEM CP.
    UINT codePage = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Console", L"CodePage", RRF_RT_REG_DWORD,
                     nullptr, &value, &size)
        == ERROR_SUCCESS) {
        codePage = UINT(value);
    }
    if (codePage == 0)
        codePage = GetOEMCP();

    // Qt 6 has no decoder for an arbitrary code page, so this goes through
    // MultiByteToWideChar directly.
    const int needed = MultiByteToWideChar(codePage, 0, bytes.constData(), int(bytes.size()),
                                           nullptr, 0);
    if (needed <= 0)
        return QString::fromLocal8Bit(bytes);   // unconvertible; the old answer is no worse

    QVarLengthArray<wchar_t> wide(needed);
    MultiByteToWideChar(codePage, 0, bytes.constData(), int(bytes.size()), wide.data(), needed);
    return QString::fromWCharArray(wide.data(), needed);
#else
    return QString::fromLocal8Bit(bytes);
#endif
}

} // namespace Console
