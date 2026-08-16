#include "updater.h"
#include "i18n.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {
const QString Owner = QStringLiteral("shadesofdeath");
const QString Repo = QStringLiteral("Arbitrium");
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

void Updater::check()
{
    if (m_busy)
        return;
    m_busy = true;

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
                                Locale::tr(QStringLiteral("err.noRelease")));
                return;
            }
            Q_EMIT finished(false, QString(), releasesUrl(), reply->errorString());
            return;
        }

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty()) {
            Q_EMIT finished(false, QString(), releasesUrl(), Locale::tr(QStringLiteral("err.badResponse")));
            return;
        }

        const QString url = release.value(QStringLiteral("html_url")).toString(releasesUrl());
        const bool newer = compareVersions(tag, QCoreApplication::applicationVersion()) > 0;
        Q_EMIT finished(newer, tag, url, QString());
    });
}
