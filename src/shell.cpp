#include "shell.h"

#include "catalog.h"

#include <QProcess>
#include <QRegularExpression>
#include <QThread>

namespace Shell {

bool needsExplorerRestart(const Tweak &tweak)
{
    // Matched against the key path rather than stored per tweak: the catalogue is
    // generated from the tutorials, and this rule stays correct as it grows.
    static const QRegularExpression shellKeys(
        QStringLiteral("CurrentVersion\\\\(Explorer|Policies\\\\Explorer)"
                       "|\\\\Explorer\\\\Advanced"
                       "|ContentDeliveryManager"
                       "|Windows\\\\Shell"
                       "|\\\\DWM\\b"
                       "|Control Panel\\\\Desktop"
                       "|Themes\\\\Personalize"
                       "|\\\\Feeds\\b"
                       "|\\\\Search\\b"
                       "|CLSID"
                       "|\\\\Dsh\\b"),
        QRegularExpression::CaseInsensitiveOption);

    for (const RegistryEntry &entry : tweak.reg)
        if (shellKeys.match(entry.path).hasMatch())
            return true;
    return false;
}

bool restartExplorer(QString *error)
{
#ifdef Q_OS_WIN
    QProcess kill;
    kill.start(QStringLiteral("taskkill"),
               {QStringLiteral("/F"), QStringLiteral("/IM"), QStringLiteral("explorer.exe")});
    if (!kill.waitForFinished(8000)) {
        if (error)
            *error = QStringLiteral("explorer.exe durdurulamadı");
        return false;
    }

    // Windows usually brings the shell back by itself (AutoRestartShell), but not when
    // it was force-killed, and not at all on every edition — so start it explicitly.
    // A short pause first, otherwise the new process races the old one's teardown.
    QThread::msleep(600);
    QProcess::startDetached(QStringLiteral("explorer.exe"), {});
    return true;
#else
    if (error)
        *error = QStringLiteral("yalnızca Windows");
    return false;
#endif
}

} // namespace Shell
