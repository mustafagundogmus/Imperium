// updater.h — look, ask, download, verify, replace, restart.
//
// The check itself is what it always was: one HTTPS GET to the public releases endpoint,
// no token, no telemetry, nothing about this machine leaving it but the User-Agent the
// request has to carry anyway.
//
// What follows the check is new, and the paragraph that used to stand here — "it never
// downloads or installs anything, it hands the user the release page and lets them
// decide" — is no longer true, so here is what the code does now. When a check finds a
// newer version the user is shown an offer naming both versions with the release notes
// behind it. If they accept, this class fetches that release's own
// Arbitrium-v<version>-win64.exe together with the .sha256 published beside it, compares
// the digest of what arrived against the line for that file, writes the bytes next to the
// running executable, renames the running executable out of the way, moves the new one
// into its place, starts it, and asks the application to quit. The program replaces
// itself.
//
// Five things stayed true, which is why the promise could be narrowed instead of dropped:
//
//   * Nothing moves without a human. The launch check may look and may ask; only a press
//     on "Şimdi güncelle" downloads a byte. No silent download, no silent replacement,
//     nothing running in the background.
//   * Nothing is installed. No installer, no service, no scheduled task, not one registry
//     key. The folder ends up holding the same executable at the same path with newer
//     bytes in it. The displaced copy gets one fixed file name that the next start
//     deletes — one name, so a folder can never fill up with old copies.
//   * Every URL is checked before it is used and every redirect before it is followed.
//     The repository is pinned in repository(); an asset published anywhere else is not
//     fetched, and a redirect out of GitHub is refused rather than followed.
//   * The SHA-256 proves the download arrived whole and is the file that release
//     published. It proves nothing about who built it. verify() in updater.cpp is where
//     that limit is written down in full, and the user is told the same thing in the
//     offer before they agree to anything.
//   * Every failure names the step it failed at and the reason the operating system gave,
//     and all but one of them leave the user with the application they already had, at the
//     path it was already at, still running. The exception is real and is not smoothed
//     over: if the running executable has been renamed out of the way and then cannot be
//     put back — two file operations, both of which the swap in verifyAndPlace() has to do
//     in that order — the program is still running but its file is now called
//     Arbitrium.exe.old. "update.fail.stranded" is the one message that asks the user to
//     do something, and it names both paths so they can do it. Everything that can be
//     arranged to happen before that point does: the folder is proved writable before the
//     offer, the download is held in memory until its digest matches, and the new bytes
//     are flushed to the disk before the old file is touched at all.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class Updater : public QObject
{
    Q_OBJECT

public:
    /// One published release, reduced to the six things this program needs from it.
    /// Assembled by check() and handed back by latest(); the two URLs are already through
    /// the scheme/host/path check, so an empty one means "the release did not publish
    /// this, or published it somewhere we do not fetch from".
    struct Release
    {
        QString version;    ///< tag_name as published, e.g. "v0.10.1"
        QString pageUrl;    ///< html_url, or the releases page when that failed the check
        QString notes;      ///< the release body, shown behind the offer's Details button
        QString exeName;    ///< "Arbitrium-v0.10.1-win64.exe" — also the sums line to match
        QUrl exe;
        QUrl sums;

        /// True when the executable asset is present under the pinned repository.
        bool installable() const { return !exe.isEmpty(); }
    };

    /// Which part of the update is running. The dialog shows this, and a failure names
    /// it, because "the update failed" without a stage is not a bug report.
    enum class Stage { Download, Verify, Install };

    explicit Updater(QObject *parent = nullptr);

    /// Owner/repo the release check reads from.
    static QString repository();
    static QString releasesUrl();

    /// \a userInitiated marks a check somebody asked for by pressing the button. The
    /// launch-time check is not one, and the difference still decides how loud the result
    /// is allowed to be: a failed check nobody asked for says nothing at all. It no longer
    /// decides whether a *found* update is announced — both kinds of check now put the
    /// same offer on screen, because an update the user is never told about is the one
    /// thing this feature exists to stop. Carried through the signal rather than held in
    /// a member so a second check cannot inherit the first one's origin.
    void check(bool userInitiated = false);
    bool busy() const { return m_busy; }

    /// The release the last successful check saw, valid only while updateAvailable was
    /// true. Not carried in the signal because, unlike userInitiated, it is filled in
    /// immediately before finished() is emitted and cannot belong to another check.
    const Release &latest() const { return m_latest; }

    /// Whether the automatic launch check is due. Settings decides whether it happens at
    /// all; this only stops it happening more than once a day, so a portable tool that
    /// gets opened eight times in an afternoon still asks GitHub once.
    static bool launchCheckDue();

    /// Compares two "1.2.3" style versions. Returns >0 when \a a is newer than \a b.
    static int compareVersions(const QString &a, const QString &b);

    // --- the folder this program lives in -----------------------------------------

    /// Directory holding the running executable — the one an update writes into.
    static QString installFolder();

    /// Where the running executable is moved to while the new one takes its place, and
    /// where the new one is written before that happens. Both are derived from the
    /// current path, so renaming the executable renames these with it.
    static QString leftoverPath();
    static QString stagedPath();

    /// Can this process write into installFolder()? Probed by actually creating a file
    /// there, not guessed from the path: the app is manifested requireAdministrator, so
    /// Program Files usually *is* writable for it, while a copy run from a read-only
    /// share, a mounted ISO or a write-protected stick is not. Called before the offer,
    /// so a folder that cannot take the new binary is reported instead of downloading
    /// fifteen megabytes to discover it at the end.
    static bool installFolderWritable(QString *error = nullptr);

    /// Deletes what a previous update left behind. Returns false when something is still
    /// there — normally because the process being replaced has not finished exiting yet,
    /// which is why MainWindow tries this twice.
    static bool sweepPreviousInstall();

    // --- installing ----------------------------------------------------------------

    /// Downloads \a release, verifies it, puts it in place and starts it. Reports every
    /// step through the signals below and does exactly nothing else on failure.
    void install(const Release &release);
    void cancelInstall();
    bool installing() const { return m_installing; }

Q_SIGNALS:
    /// \a version is empty when the check failed; \a error then says why.
    /// \a userInitiated is the flag the check that produced this was started with.
    void finished(bool updateAvailable, const QString &version, const QString &url,
                  const QString &error, bool userInitiated);

    void stageChanged(Stage stage);
    void progress(qint64 received, qint64 total);

    /// \a message is a finished sentence naming the stage and what it left behind.
    void installFailed(const QString &message);

    /// The new executable is in place and running. The only correct response is to quit:
    /// this process is now holding the old binary open, and nothing else can clean it up.
    void installReady();

private:
    /// Where gateRedirects() writes down a host it declined to follow, so the transfer's
    /// own finished handler can tell that refusal from the user pressing Cancel — abort()
    /// reports both as OperationCanceledError and the two must not say the same thing.
    ///
    /// One of these per transfer rather than one member per Updater, which is what it was.
    /// A release check and an install's two downloads can be in flight together, they all
    /// go through the same gate, and a single member let whichever refused last describe
    /// somebody else's failure. Shared with the reply's own lambdas so it lives exactly as
    /// long as the transfer that produced it.
    using RefusedHost = std::shared_ptr<QString>;

    void fetchSums();
    void fetchExe();
    void verifyAndPlace(const QByteArray &payload);
    void failInstall(const QString &message);
    [[nodiscard]] RefusedHost gateRedirects(QNetworkReply *reply);
    void prepare(QNetworkRequest &request, int timeoutMs) const;

    QNetworkAccessManager *m_network = nullptr;
    bool m_busy = false;
    bool m_pendingUserInitiated = false;   ///< origin of the check in flight
    Release m_latest;

    Release m_target;                      ///< the release install() is working on
    QNetworkReply *m_transfer = nullptr;   ///< the download in flight, for cancelInstall()
    QByteArray m_sums;
    bool m_installing = false;
    bool m_cancelled = false;
};
