#include "updater.h"
#include "i18n.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryFile>
#include <QUrl>

#ifdef Q_OS_WIN
#  include <io.h>
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {
const QString Owner = QStringLiteral("shadesofdeath");
const QString Repo = QStringLiteral("Arbitrium");

/// When the automatic check last completed, so the launch check can be held to once a
/// day. Deliberately not a member of Settings: that class holds the four preferences the
/// settings page offers, and this is not a preference — it is a marker the user never
/// sets and never sees, in the same QSettings key the rest of the app writes to.
const QString KeyLastCheck = QStringLiteral("app/lastUpdateCheck");

constexpr qint64 OneDaySeconds = 24 * 60 * 60;

/// The one address in the program that is not written into the program.
///
/// Every other URL handed to QDesktopServices::openUrl is a literal in the source. This
/// one arrives in a JSON body off the network and is opened by a process that always runs
/// as administrator, and openUrl on Windows is ShellExecute — so a scheme this program
/// never meant to launch would be launched, elevated, by the shell. GitHub's own html_url
/// is always https on github.com; anything else is either not GitHub answering or not the
/// answer it meant to give. The releases page is the honest substitute in both cases,
/// because it is where the user was being sent anyway.
/// Whether \a url carries no port of its own, or the one https already means. Checked
/// separately from the host everywhere below, because QUrl::host() answers "github.com"
/// just as happily for https://github.com:8443/… — the authority the two checks together
/// are meant to pin is the host *and* the port, and leaving the second half out would make
/// the pin something the rule never said it was.
bool defaultHttpsPort(const QUrl &url)
{
    return url.port() == -1 || url.port() == 443;
}

QString trustedReleaseUrl(const QString &candidate)
{
    const QUrl url(candidate);
    if (url.scheme() == QLatin1String("https") && url.host() == QLatin1String("github.com")
        && defaultHttpsPort(url))
        return candidate;
    return Updater::releasesUrl();
}

/// The same discipline, applied to the two URLs whose bytes are about to be written next
/// to the running executable and then executed — where it matters a great deal more than
/// it does for a browser tab.
///
/// browser_download_url arrives in the same JSON body as everything else, so it is not
/// evidence of anything on its own. What makes it usable is that the answer is fully
/// determined: a release asset of this repository lives at exactly
/// https://github.com/<owner>/<repo>/releases/download/<tag>/<name> and nowhere else. Any
/// other scheme, host or path prefix means the reply is not describing an asset of the
/// pinned repository, and an empty QUrl comes back rather than a fetch.
QUrl trustedAssetUrl(const QString &candidate)
{
    // Dot segments are resolved before the path is looked at, and the normalised URL is
    // the one that goes on the wire. Otherwise the prefix test is a test of spelling
    // rather than of destination: ".../releases/download/../../foo/raw/main/x.exe" starts
    // with the prefix, and the server — which does resolve it — would answer with
    // something from an entirely different part of github.com, redirect it to
    // raw.githubusercontent.com, and the redirect gate below would let it through because
    // that host is on the list. The URL this program actually uses and the URL it actually
    // checked have to be the same string.
    const QUrl url = QUrl(candidate).adjusted(QUrl::NormalizePathSegments);
    if (url.scheme() != QLatin1String("https") || url.host() != QLatin1String("github.com")
        || !defaultHttpsPort(url))
        return {};
    const QString prefix = QStringLiteral("/%1/%2/releases/download/").arg(Owner, Repo);
    if (!url.path().startsWith(prefix))
        return {};
    return url;
}

/// Hosts a redirect may land on.
///
/// The asset URL above is stable and checked, but it is not where the bytes come from:
/// github.com answers a release download with a 302 into GitHub's object store, whose
/// host is not github.com and whose name has changed more than once. Refusing every
/// cross-host redirect would refuse every download; following whatever arrives would undo
/// the point of checking the URL in the first place. So the redirect target is checked
/// too, against the only three shapes GitHub uses, and anything else aborts the transfer
/// with a message that names the host it refused.
bool redirectAllowedTo(const QUrl &url)
{
    if (url.scheme() != QLatin1String("https") || !defaultHttpsPort(url))
        return false;
    const QString host = url.host().toLower();
    return host == QLatin1String("github.com")
           || host == QLatin1String("api.github.com")
           || host.endsWith(QLatin1String(".githubusercontent.com"));
}

/// Pulls the digest for \a name out of sha256sum output. The release workflow writes the
/// file with `sha256sum a b > file`, so each line is "<64 hex><two spaces><name>"; the
/// leading '*' of the binary-mode variant is tolerated because it costs one line and
/// coreutils writes it on other platforms.
QByteArray digestFor(const QByteArray &sums, const QString &name)
{
    const QByteArray wanted = name.toUtf8();
    const QList<QByteArray> lines = sums.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        const int gap = line.indexOf(' ');
        if (gap != 64)
            continue;
        QByteArray file = line.mid(gap).trimmed();
        if (file.startsWith('*'))
            file = file.mid(1);
        if (file == wanted)
            return line.left(64).toLower();
    }
    return {};
}

/// Pushes the staged bytes past the operating system's write cache before the running
/// executable is renamed out of the way.
///
/// QFileDevice::flush() only hands the bytes to Windows; it says nothing about when they
/// reach the disk. The rename that follows it is an NTFS metadata change, and metadata is
/// journaled while file data is not ordered against it — so a power cut in the moment
/// between the two is able to leave the directory entry Arbitrium.exe pointing at a file
/// whose contents never landed. That is the one way this feature could take the
/// application away from somebody: a program that no longer starts, with its only working
/// copy sitting under a .old name nobody has told them about. FlushFileBuffers closes the
/// window, so a crash at any point after this returns leaves either the old binary or the
/// new one and never half of either.
///
/// Failing it is treated as fatal to the update by the caller, which does introduce a
/// refusal that did not exist before. That is the trade taken on purpose: a volume this
/// does not work on is one where the next two lines cannot be promised to be reversible,
/// and refusing there costs a user an update while going ahead anyway could cost them the
/// program. Every local NTFS, exFAT and SMB volume implements it.
///
/// Empty on success, the reason otherwise, like the two helpers below it.
QString flushToDiskOrReason(QFile &file)
{
#ifdef Q_OS_WIN
    const int fd = file.handle();
    const HANDLE handle = fd < 0 ? INVALID_HANDLE_VALUE
                                 : reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE)
        return QStringLiteral("FlushFileBuffers: no handle");
    if (FlushFileBuffers(handle))
        return {};
    // The bare Win32 code rather than a sentence. Nothing this program could say about it
    // would be more use to whoever ends up reading the bug report than the number is.
    return QStringLiteral("FlushFileBuffers: %1").arg(GetLastError());
#else
    return file.flush() ? QString() : file.errorString();
#endif
}

/// The three file operations of the swap, each answering with the reason it failed rather
/// than with a bare false.
///
/// The static QFile::rename/remove overloads throw the reason away, and every branch that
/// calls these is one the user has to do something about — a scanner holding the new file,
/// a folder that went read-only, an older copy still running from the leftover. "Could not
/// put the new version in place: C:\Tools\Arbitrium.exe" is not a thing anybody can act
/// on; the same line with "Access is denied" after it is. The path and the reason are
/// joined here rather than in the translation table because an em dash is punctuation, not
/// a sentence somebody has to translate ten times.
///
/// Both return an empty string on success, so the caller reads as "if there is a reason,
/// stop and say it".
QString joinReason(const QString &path, const QString &reason)
{
    return QStringLiteral("%1 — %2").arg(QDir::toNativeSeparators(path), reason);
}

QString removeOrReason(const QString &path)
{
    QFile file(path);
    if (!file.exists() || file.remove())
        return {};
    return joinReason(path, file.errorString());
}

QString renameOrReason(const QString &from, const QString &to)
{
    QFile file(from);
    if (file.rename(to))
        return {};
    return joinReason(from, file.errorString());
}
} // namespace

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

bool Updater::launchCheckDue()
{
    const QDateTime last = QSettings().value(KeyLastCheck).toDateTime();
    if (!last.isValid())
        return true;
    // Both directions, because a clock that has been moved backwards — a fresh Windows
    // install before it has talked to a time server, a dual boot, a VM restored from a
    // snapshot — would otherwise park a date in the future in the registry and stop the
    // launch check for as long as it took the real time to catch up.
    const qint64 elapsed = last.secsTo(QDateTime::currentDateTimeUtc());
    return elapsed < 0 || elapsed >= OneDaySeconds;
}

void Updater::prepare(QNetworkRequest &request, int timeoutMs) const
{
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Arbitrium/%1").arg(QCoreApplication::applicationVersion()));
    // Not NoLessSafeRedirectPolicy: that would follow an https redirect to any host at
    // all, which is exactly the decision redirectAllowedTo() exists to make. This policy
    // stops at every hop and waits for redirectAllowed() to be emitted.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::UserVerifiedRedirectPolicy);
    // An idle timeout rather than a deadline: Qt restarts it whenever bytes arrive, so a
    // fifteen-megabyte download on a slow line is fine and a stalled one still gives up.
    request.setTransferTimeout(timeoutMs);
}

Updater::RefusedHost Updater::gateRedirects(QNetworkReply *reply)
{
    auto refused = std::make_shared<QString>();
    // Bound to the reply, not to this: the lambda touches nothing else and has no reason
    // to outlive the transfer it is gating.
    connect(reply, &QNetworkReply::redirected, reply, [reply, refused](const QUrl &target) {
        if (redirectAllowedTo(target)) {
            Q_EMIT reply->redirectAllowed();
            return;
        }
        // Recorded before the abort, because abort() surfaces as OperationCanceledError —
        // the same error the user's own Cancel produces — and the two must not report the
        // same thing.
        *refused = target.host();
        reply->abort();
    });
    return refused;
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
    prepare(request, 10000);

    QNetworkReply *reply = m_network->get(request);
    const RefusedHost refused = gateRedirects(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, refused] {
        reply->deleteLater();
        m_busy = false;
        m_latest = Release();

        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            // A repo with no published release answers 404; that is not a failure.
            if (status == 404) {
                Q_EMIT finished(false, QString(), releasesUrl(),
                                Locale::tr(QStringLiteral("err.noRelease")), m_pendingUserInitiated);
                return;
            }
            // A refused redirect names the host, here as well as in the two downloads. It
            // used to fall through to errorString(), which for an abort() is "Operation
            // canceled" — a sentence that tells the user they cancelled something they did
            // not, about the one failure that is worth them knowing the detail of.
            const QString why = refused->isEmpty()
                                    ? reply->errorString()
                                    : Locale::tr(QStringLiteral("update.fail.redirect")).arg(*refused);
            Q_EMIT finished(false, QString(), releasesUrl(), why, m_pendingUserInitiated);
            return;
        }

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty()) {
            Q_EMIT finished(false, QString(), releasesUrl(),
                            Locale::tr(QStringLiteral("err.badResponse")), m_pendingUserInitiated);
            return;
        }

        // The check completed and got an answer, so the daily floor starts here. Stamped
        // for a manual check too: it answered the same question, and asking again at the
        // next launch would be asking twice for one reason.
        QSettings().setValue(KeyLastCheck, QDateTime::currentDateTimeUtc());

        m_latest.version = tag;
        m_latest.pageUrl =
            trustedReleaseUrl(release.value(QStringLiteral("html_url")).toString(releasesUrl()));
        m_latest.notes = release.value(QStringLiteral("body")).toString().trimmed();

        // The asset names are constructed rather than searched for. The release workflow
        // writes exactly Arbitrium-v<version>-win64.exe and .sha256 beside it, so the
        // program knows both names before it asks; picking "the first .exe in the list"
        // would instead let the answer decide what gets downloaded and run.
        const QString bare = QString(tag).remove(0, tag.startsWith(QLatin1Char('v')) ? 1 : 0);
        m_latest.exeName = QStringLiteral("Arbitrium-v%1-win64.exe").arg(bare);
        const QString sumsName = QStringLiteral("Arbitrium-v%1-win64.sha256").arg(bare);

        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (name != m_latest.exeName && name != sumsName)
                continue;
            const QUrl url =
                trustedAssetUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            if (name == m_latest.exeName)
                m_latest.exe = url;
            else
                m_latest.sums = url;
        }

        const bool newer = compareVersions(tag, QCoreApplication::applicationVersion()) > 0;
        Q_EMIT finished(newer, tag, m_latest.pageUrl, QString(), m_pendingUserInitiated);
    });
}

// --- the folder this program lives in ---------------------------------------------

QString Updater::installFolder()
{
    return QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
}

QString Updater::leftoverPath()
{
    // Derived from the current path and given a suffix rather than a name of its own, for
    // two reasons. It follows the executable if the user has renamed it, so an update
    // still finds its own leftover afterwards; and it does not end in .exe, so the file
    // sitting in the folder for one restart cannot be double-clicked by mistake.
    return QCoreApplication::applicationFilePath() + QLatin1String(".old");
}

QString Updater::stagedPath()
{
    return QCoreApplication::applicationFilePath() + QLatin1String(".new");
}

bool Updater::installFolderWritable(QString *error)
{
    // A real file, actually written to, actually flushed. Testing QFileInfo::isWritable()
    // on the directory answers a question about the ACL that Windows then disagrees with
    // often enough to be useless — a read-only volume, a full disk, a mounted image and a
    // folder redirected onto a share all pass it and fail the first write.
    QTemporaryFile probe(QDir(installFolder()).filePath(QStringLiteral("arbitrium-probe-XXXXXX.tmp")));
    if (!probe.open()) {
        if (error)
            *error = probe.errorString();
        return false;
    }
    const bool wrote = probe.write("arbitrium", 9) == 9 && probe.flush();
    if (!wrote && error)
        *error = probe.errorString();
    return wrote;   // QTemporaryFile removes itself on the way out either way
}

bool Updater::sweepPreviousInstall()
{
    bool clean = true;
    // The one that normally exists: the binary this process replaced last time. It cannot
    // go while the process running from it is still alive, which is why the first attempt
    // right after a self-update usually fails and MainWindow tries once more.
    if (QFile::exists(leftoverPath()))
        clean = QFile::remove(leftoverPath()) && clean;
    // And the download staging file, which only exists if the machine lost power between
    // writing it and moving it into place.
    if (QFile::exists(stagedPath()))
        clean = QFile::remove(stagedPath()) && clean;
    return clean;
}

// --- installing --------------------------------------------------------------------

void Updater::install(const Release &release)
{
    if (m_installing)
        return;
    m_installing = true;
    m_cancelled = false;
    m_sums.clear();
    m_target = release;

    // UpdateDialog::offer() refuses an uninstallable release before it asks, so this is
    // not the path anybody reaches. It is here because install() is public and what it
    // does is replace the executable: a caller added later that forgets the check would
    // otherwise get an empty QUrl handed to QNetworkAccessManager and a "protocol unknown"
    // download failure, which describes nothing. The refusal states the actual reason.
    if (!m_target.installable()) {
        failInstall(Locale::tr(QStringLiteral("update.fail.asset")));
        return;
    }

    Q_EMIT stageChanged(Stage::Download);
    fetchSums();
}

void Updater::cancelInstall()
{
    if (!m_installing)
        return;
    m_cancelled = true;
    if (m_transfer)
        m_transfer->abort();
    // Forgotten here and not in the reply's own handler, so that the handler — which runs
    // some time after abort() returns — can tell "I am the transfer this class is waiting
    // on" from "I am a transfer that was cancelled or superseded". See fetchSums().
    m_transfer = nullptr;
    m_installing = false;
    // Nothing to undo. Everything up to verification lives in memory — see
    // verifyAndPlace() — so a cancel at any point before the file is written leaves the
    // folder exactly as it was, which is the whole reason it is arranged that way.
}

void Updater::failInstall(const QString &message)
{
    m_installing = false;
    m_transfer = nullptr;
    m_sums.clear();
    Q_EMIT installFailed(message);
}

void Updater::fetchSums()
{
    QNetworkRequest request(m_target.sums);
    prepare(request, 30000);

    QNetworkReply *reply = m_network->get(request);
    m_transfer = reply;
    const RefusedHost refused = gateRedirects(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, refused] {
        reply->deleteLater();
        // Identity first, and only then the flags.
        //
        // abort() does not deliver finished() before it returns, so a cancel leaves this
        // handler queued behind it. If a second update is started in the meantime — the
        // user cancels, checks again and accepts — m_cancelled has been put back to false
        // and m_installing back to true by then, and this handler, running for a reply
        // nobody is waiting for any more, would clear the *new* transfer's pointer and
        // then fail the *new* install with "Operation canceled". Comparing the reply
        // against the one this class is actually waiting on is the only test that tells
        // the two apart, so it is the one made first.
        if (reply != m_transfer)
            return;
        m_transfer = nullptr;
        if (m_cancelled)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            if (!refused->isEmpty())
                failInstall(Locale::tr(QStringLiteral("update.fail.redirect")).arg(*refused));
            else
                failInstall(Locale::tr(QStringLiteral("update.fail.download")).arg(reply->errorString()));
            return;
        }
        // Fetched first although it is the smaller half of the answer: it is two hundred
        // bytes, and a release whose sums file is missing cannot be verified, so finding
        // that out now costs one round trip instead of a fifteen-megabyte download.
        m_sums = reply->readAll();
        fetchExe();
    });
}

void Updater::fetchExe()
{
    QNetworkRequest request(m_target.exe);
    prepare(request, 30000);

    QNetworkReply *reply = m_network->get(request);
    m_transfer = reply;
    const RefusedHost refused = gateRedirects(reply);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 received, qint64 total) {
                if (reply == m_transfer)
                    Q_EMIT progress(received, total);
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, refused] {
        reply->deleteLater();
        if (reply != m_transfer)   // cancelled or superseded — see fetchSums()
            return;
        m_transfer = nullptr;
        if (m_cancelled)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            if (!refused->isEmpty())
                failInstall(Locale::tr(QStringLiteral("update.fail.redirect")).arg(*refused));
            else
                failInstall(Locale::tr(QStringLiteral("update.fail.download")).arg(reply->errorString()));
            return;
        }
        verifyAndPlace(reply->readAll());
    });
}

/// What the SHA-256 comparison below is worth, said plainly, because overstating it would
/// be worse than not doing it.
///
/// It proves two things. The download arrived whole — no truncation, no proxy that
/// rewrote it, no half-written file — and the bytes are the ones that release published,
/// because the digest they are compared against comes from that release's own .sha256
/// asset. That is enough to know the executable about to replace this one is the file
/// GitHub is serving under this tag.
///
/// It proves nothing about who built that file. The sums asset is uploaded to the same
/// release, by the same account, through the same button as the executable: anyone able
/// to replace one is able to replace the other in the same motion, and the pair would
/// still agree. The release workflow says the same thing in the comment over the step
/// that writes it.
///
/// The thing that does establish provenance is the build attestation GitHub publishes
/// alongside the release — a Sigstore bundle binding each digest to this repository, the
/// commit and the workflow run that produced it. Verifying one in-process is deliberately
/// not attempted here: it means a Fulcio certificate chain, a Rekor inclusion proof and a
/// trust root that has to be updated, which is a signature-verification stack living
/// inside a program that is about to execute the thing it just verified. That belongs in
/// a tool built for it, and there is one — the README documents it:
///
///     gh attestation verify Arbitrium-vX.Y.Z-win64.exe --repo shadesofdeath/Arbitrium
///
/// The offer dialog tells the user this before they agree to anything, in
/// "update.proof", rather than letting a green checkmark imply more than it means.
void Updater::verifyAndPlace(const QByteArray &payload)
{
    Q_EMIT stageChanged(Stage::Verify);

    const QByteArray expected = digestFor(m_sums, m_target.exeName);
    const QByteArray actual =
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
    if (expected.isEmpty() || expected != actual) {
        // Two different refusals, answered with one sentence on purpose. Either the sums
        // asset carries no line for this file at all — a release built by a workflow that
        // did not publish one, or published one listing only the .zip — or it carries a
        // line and the bytes do not match it. "update.fail.verify" is worded to be true of
        // both ("could not be verified against the digest this release published", not
        // "does not match it"), because a message that claimed a mismatch where none was
        // published would be describing something that did not happen, and the user's next
        // move is the same either way: do not trust this download, go to the release page.
        //
        // The payload is a local that is about to go out of scope, so refusing here means
        // the bytes are dropped without ever having touched the disk. Nothing to clean up
        // and nothing that could be run by accident afterwards.
        failInstall(Locale::tr(QStringLiteral("update.fail.verify")));
        return;
    }

    Q_EMIT stageChanged(Stage::Install);

    const QString current = QCoreApplication::applicationFilePath();
    const QString staged = stagedPath();
    const QString leftover = leftoverPath();

    // A leftover from a previous update has to go before this one can make another, and
    // there is only ever the one name for it. An update that cannot clear it stops here
    // rather than inventing ".old2": a portable program must not turn its own folder into
    // a pile of its own past versions, and the only thing that normally holds this file is
    // a copy of Arbitrium still running from it, which the user can close.
    QString why = removeOrReason(leftover);
    if (why.isEmpty())
        why = removeOrReason(staged);
    if (!why.isEmpty()) {
        failInstall(Locale::tr(QStringLiteral("update.fail.install")).arg(why));
        return;
    }

    QFile out(staged);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        failInstall(Locale::tr(QStringLiteral("update.fail.install"))
                        .arg(joinReason(staged, out.errorString())));
        return;
    }
    // Written, handed to Windows, and then actually on the disk — in that order, and the
    // third one is not optional: the rename below is only reversible while the bytes it
    // renames over are real. See flushToDiskOrReason().
    const bool stored = out.write(payload) == payload.size() && out.flush();
    const QString writeError = stored ? flushToDiskOrReason(out) : out.errorString();
    out.close();
    if (!stored || !writeError.isEmpty()) {
        QFile::remove(staged);
        failInstall(Locale::tr(QStringLiteral("update.fail.install"))
                        .arg(joinReason(staged, writeError)));
        return;
    }

    // Windows will not let a running executable be overwritten, but it will let it be
    // renamed: the process keeps its image through the rename, and the path it used to
    // occupy comes free. That is the whole trick this feature turns on.
    why = renameOrReason(current, leftover);
    if (!why.isEmpty()) {
        QFile::remove(staged);
        failInstall(Locale::tr(QStringLiteral("update.fail.install")).arg(why));
        return;
    }

    why = renameOrReason(staged, current);
    if (!why.isEmpty()) {
        // The destination is free — we just emptied it — so this failing means something
        // outside this program is interfering. Put ours back before saying so.
        if (!renameOrReason(leftover, current).isEmpty()) {
            failInstall(Locale::tr(QStringLiteral("update.fail.stranded"))
                            .arg(QDir::toNativeSeparators(leftover),
                                 QDir::toNativeSeparators(current)));
            return;
        }
        QFile::remove(staged);
        failInstall(Locale::tr(QStringLiteral("update.fail.install")).arg(why));
        return;
    }

    // This process is elevated — the manifest asks for it and Windows granted it before
    // main() ran — so the child inherits an elevated token and CreateProcess succeeds
    // without a second UAC prompt, even though the new executable's own manifest also
    // says requireAdministrator. From a non-elevated parent this same call would fail
    // with ERROR_ELEVATION_REQUIRED, which is a case this app cannot be in.
    //
    // The child is given the install folder as its working directory rather than
    // inheriting ours, so it behaves exactly as it would if it had been double-clicked.
    if (!QProcess::startDetached(current, {}, installFolder())) {
        // Undo the whole swap: the new file is not running, so it goes back to the staging
        // name and the old one goes back to the path this process is still executing from.
        //
        // Moved rather than deleted, which is what this did first. A delete that fails —
        // and the likely reason to be here at all is a scanner that has just watched an
        // executable appear and is holding it — leaves the destination occupied, so the
        // rename after it is then guaranteed to fail too and a rollback that had one way
        // to work is left with none. A move needs the same access as the delete and, when
        // it works, also keeps the fifteen megabytes that were already verified; the next
        // start sweeps the staging file either way.
        if (!renameOrReason(current, staged).isEmpty()
            || !renameOrReason(leftover, current).isEmpty()) {
            failInstall(Locale::tr(QStringLiteral("update.fail.stranded"))
                            .arg(QDir::toNativeSeparators(leftover),
                                 QDir::toNativeSeparators(current)));
            return;
        }
        QFile::remove(staged);
        failInstall(Locale::tr(QStringLiteral("update.fail.install"))
                        .arg(QDir::toNativeSeparators(current)));
        return;
    }

    // A process's current directory is an open handle on that directory. Double-clicking
    // the executable makes that handle point at the install folder, and it outlives every
    // file operation above — so it is dropped here, before the quit, and the folder is
    // left held open by nobody but the new process.
    QDir::setCurrent(QDir::tempPath());

    m_installing = false;
    Q_EMIT installReady();
}
