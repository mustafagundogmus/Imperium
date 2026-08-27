// updater.h — "Güncellemeleri denetle" against the project's GitHub releases.
//
// One HTTPS GET to the public releases endpoint, no token, no telemetry sent. The check
// only ever *reports*: it never downloads or installs anything, it hands the user the
// release page and lets them decide.

#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(QObject *parent = nullptr);

    /// Owner/repo the release check reads from.
    static QString repository();
    static QString releasesUrl();

    /// \a userInitiated marks a check somebody asked for by pressing the button. The
    /// launch-time check is not one, and the difference decides whether finding a new
    /// version is allowed to open a browser: doing that unprompted, seconds after the
    /// window appears, is not what "checks quietly while the app opens" describes.
    /// Carried through the signal rather than held in a member so a second check cannot
    /// inherit the first one's origin.
    void check(bool userInitiated = false);
    bool busy() const { return m_busy; }

    /// Compares two "1.2.3" style versions. Returns >0 when \a a is newer than \a b.
    static int compareVersions(const QString &a, const QString &b);

Q_SIGNALS:
    /// \a version is empty when the check failed; \a error then says why.
    /// \a userInitiated is the flag the check that produced this was started with.
    void finished(bool updateAvailable, const QString &version, const QString &url,
                  const QString &error, bool userInitiated);

private:
    QNetworkAccessManager *m_network = nullptr;
    bool m_busy = false;
    bool m_pendingUserInitiated = false;   ///< origin of the check in flight
};
