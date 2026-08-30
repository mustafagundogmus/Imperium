#include "updater.h"
#include "i18n.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace {
const QString Owner = QStringLiteral("shadesofdeath");
const QString Repo = QStringLiteral("Arbitrium");

/// The one address in the program that is not written into the program.
///
/// Every other URL handed to QDesktopServices::openUrl is a literal in the source. This
/// one arrives in a JSON body off the network and is opened by a process that always runs
/// as administrator, and openUrl on Windows is ShellExecute — so a scheme this program
/// never meant to launch would be launched, elevated, by the shell. GitHub's own html_url
/// is always https on github.com; anything else is either not GitHub answering or not the
/// answer it meant to give. The releases page is the honest substitute in both cases,
/// because it is where the user was being sent anyway.
QString trustedReleaseUrl(const QString &candidate)
{
    const QUrl url(candidate);
    if (url.scheme() == QLatin1String("https") && url.host() == QLatin1String("github.com"))
        return candidate;
    return Updater::releasesUrl();
}
}

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString Updater::repository()
{
    return Owner + QLatin1Char('/') + Repo;
}

QString Updater::releasesUrl()
{
    return QStringLiteral("https://github.com/%1/%2/releases").arg(Owner, Repo);
}

int Updater::compareVersions(const QString &a, const QString &b)
{
    static const QRegularExpression sep(QStringLiteral("[^0-9]+"));
    const QStringList lhs = QString(a).remove(QLatin1Char('v')).split(sep, Qt::SkipEmptyParts);
    const QStringList rhs = QString(b).remove(QLatin1Char('v')).split(sep, Qt::SkipEmptyParts);

    for (int i = 0; i < qMax(lhs.size(), rhs.size()); ++i) {
        const int l = i < lhs.size() ? lhs.at(i).toInt() : 0;
        const int r = i < rhs.size() ? rhs.at(i).toInt() : 0;
        if (l != r)
            return l < r ? -1 : 1;
    }
    return 0;
}

void Updater::check(bool userInitiated)
{
    if (m_busy) {
        // A button press that lands while the launch-time check is still open is not
        // lost — it adopts the request already in flight, which is answering the same
        // question. Otherwise its reply came back marked silent and the settings row,
        // set to "Denetleniyor…" by the press, was never rewritten.
        m_pendingUserInitiated = m_pendingUserInitiated || userInitiated;
        return;
    }
    m_busy = true;
    m_pendingUserInitiated = userInitiated;

    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                                     .arg(Owner, Repo)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Arbitrium/%1").arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10000);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            // A repo with no published release answers 404; that is not a failure.
            if (status == 404) {
                Q_EMIT finished(false, QString(), releasesUrl(),
                                Locale::tr(QStringLiteral("err.noRelease")), m_pendingUserInitiated);
                return;
            }
            Q_EMIT finished(false, QString(), releasesUrl(), reply->errorString(), m_pendingUserInitiated);
            return;
        }

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty()) {
            Q_EMIT finished(false, QString(), releasesUrl(),
                            Locale::tr(QStringLiteral("err.badResponse")), m_pendingUserInitiated);
            return;
        }

        const QString url =
            trustedReleaseUrl(release.value(QStringLiteral("html_url")).toString(releasesUrl()));
        const bool newer = compareVersions(tag, QCoreApplication::applicationVersion()) > 0;
        Q_EMIT finished(newer, tag, url, QString(), m_pendingUserInitiated);
    });
}
