#include "winpaths.h"

#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace WinPaths {

QString system32()
{
#ifdef Q_OS_WIN
    wchar_t buffer[MAX_PATH] = {};
    const UINT written = GetSystemDirectoryW(buffer, MAX_PATH);
    if (written > 0 && written < MAX_PATH)
        return QString::fromWCharArray(buffer, int(written));
#endif
    return QStringLiteral("C:\\Windows\\System32");
}

QString windows()
{
#ifdef Q_OS_WIN
    wchar_t buffer[MAX_PATH] = {};
    const UINT written = GetWindowsDirectoryW(buffer, MAX_PATH);
    if (written > 0 && written < MAX_PATH)
        return QString::fromWCharArray(buffer, int(written));
#endif
    return QStringLiteral("C:\\Windows");
}

QString powershell()
{
    // Cached because the two probes and every action ask for it, and because the point of
    // this function is to not touch the filesystem more than once at startup. A machine
    // does not grow a PowerShell while the application is running, and if it did, the
    // answer this returns would still be the safe one.
    static const QString resolved = [] {
        const QString path =
            system32() + QStringLiteral("\\WindowsPowerShell\\v1.0\\powershell.exe");
        return QFileInfo::exists(path) ? path : QString();
    }();
    return resolved;
}

} // namespace WinPaths
