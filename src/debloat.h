// debloat.h — the apps Windows preinstalls, as the machine itself reports them.
//
// The source is Get-AppxProvisionedPackage: the set staged in the Windows image and handed
// to every account that is created, which is what "preinstalled bloat" actually means.
// Get-AppxPackage would be the wrong question — it answers "what is registered right now",
// which sweeps in the shell's own system apps and everything the user installed themselves.
//
// There is no curated list of apps here, and deliberately so: a hand-written catalogue can
// only ever show the apps someone thought to name, which is a small and permanently stale
// fraction of what a given image ships. The names come from the Start menu (already
// localised by Windows), the logos from each package's own AppxManifest.xml.
//
// What may be removed is derived from the machine too: Windows itself marks packages
// NonRemovable and stamps core components with a System signature. Those are shown but
// locked rather than hidden, so the page is an honest inventory instead of a filtered one.
// The only additional lock is on shared runtimes, which are libraries other apps link
// against rather than apps in their own right (see criticalPrefixes in the .cpp) — a
// safety guard, not a source of data.

#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QVector>

class QProcess;

/// One installed package, as reported by this machine.
struct InstalledApp
{
    QString packageName;      ///< Get-AppxPackage Name, e.g. "Microsoft.BingWeather"
    QString packageFullName;  ///< the exact string Remove-AppxPackage needs
    QString installLocation;
    QString version;
    QString displayName;      ///< Start-menu name, manifest name, or the package name
    QString publisher;        ///< manifest PublisherDisplayName, when it carries one
    QPixmap logo;             ///< null if the manifest carried no usable logo

    bool installed = false;       ///< also registered for a user right now, not just staged
    bool userFacing = false;      ///< has a Start-menu entry — a real app, not a component
    bool systemComponent = false; ///< System-signed or flagged NonRemovable by Windows
    bool removable = false;       ///< safe to offer a Kaldır button for

    /// Which of the page's three sections this belongs in. Derived, never stored.
    int section() const
    {
        if (!installed)
            return 2;   // staged in the image, but not registered for anyone yet
        return userFacing ? 0 : 1;
    }
};

/// Enumerates every installed package in the background and resolves each one's real name
/// and logo. Cheap enough to simply re-run after a removal rather than patch in place.
class DebloatScanner : public QObject
{
    Q_OBJECT

public:
    explicit DebloatScanner(QObject *parent = nullptr);

    void start();
    bool running() const { return m_process != nullptr; }

Q_SIGNALS:
    void finished(const QVector<InstalledApp> &apps);

private:
    QProcess *m_process = nullptr;
};

namespace DebloatActions {

/// The script that removes every package named in \a packageNames: for every user account
/// already on the machine, and deprovisioned so Windows does not hand it to a new one.
/// Errors from an individual package (already gone, in use) are swallowed — a partial
/// batch is still a successful one for whatever it did manage.
QString removalScript(const QStringList &packageNames);

} // namespace DebloatActions
