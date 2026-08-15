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

    void check();
    bool busy() const { return m_busy; }

    /// Compares two "1.2.3" style versions. Returns >0 when \a a is newer than \a b.
    static int compareVersions(const QString &a, const QString &b);

Q_SIGNALS:
    /// \a version is empty when the check failed; \a error then says why.
    void finished(bool updateAvailable, const QString &version, const QString &url,
                  const QString &error);

private:
    QNetworkAccessManager *m_network = nullptr;
    bool m_busy = false;
};
