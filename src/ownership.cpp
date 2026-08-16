#include "ownership.h"
#include "i18n.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <aclapi.h>
#  include <sddl.h>
#endif

namespace {

// Principals are named by SID throughout. This machine calls the Administrators group
// "Yöneticiler", and an icacls line naming a group that does not exist under the spelling
// it was given fails without granting anything.
const QString AdminsSid = QStringLiteral("S-1-5-32-544");
const QString TrustedInstallerSid =
    QStringLiteral("S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464");

/// The SID of whoever is running us. Elevation does not change the user — it changes
/// which of their groups are enabled — so this is the account that will still be sitting
/// in front of Explorer afterwards.
QString currentUserSid()
{
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};

    DWORD needed = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    QByteArray buffer(int(needed), '\0');
    QString out;
    if (needed > 0 && GetTokenInformation(token, TokenUser, buffer.data(), needed, &needed)) {
        const auto *info = reinterpret_cast<const TOKEN_USER *>(buffer.constData());
        LPWSTR text = nullptr;
        if (ConvertSidToStringSidW(info->User.Sid, &text)) {
            out = QString::fromWCharArray(text);
            LocalFree(text);
        }
    }
    CloseHandle(token);
    return out;
#else
    return {};
#endif
}

/// Who owns \a path right now, as a SID string. This is the only trustworthy way to know
/// whether the job was done: icacls reports success in its exit code even when it could
/// not touch the file, and its printed summary is the wrong thing to parse for it.
QString ownerSid(const QString &path)
{
#ifdef Q_OS_WIN
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PSID owner = nullptr;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();

    if (GetNamedSecurityInfoW(native.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                              &owner, nullptr, nullptr, nullptr, &descriptor)
        != ERROR_SUCCESS) {
        return {};
    }

    QString out;
    LPWSTR text = nullptr;
    if (owner && ConvertSidToStringSidW(owner, &text)) {
        out = QString::fromWCharArray(text);
        LocalFree(text);
    }
    if (descriptor)
        LocalFree(descriptor);
    return out;
#else
    Q_UNUSED(path);
    return {};
#endif
}

struct Run
{
    int code = -1;
    QString output;
};

/// Runs one of the console tools without letting a window flash on screen.
Run run(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif

    process.start();
    Run result;
    if (!process.waitForStarted(5000)) {
        result.output = Locale::tr(QStringLiteral("own.startFailed")).arg(program);
        return result;
    }
    // Long enough for a deep folder; a system directory can take a while.
    if (!process.waitForFinished(120000)) {
        process.kill();
        result.output = Locale::tr(QStringLiteral("own.timeout")).arg(program);
        return result;
    }

    result.code = process.exitCode();
    result.output = QString::fromLocal8Bit(process.readAll()).trimmed();
    return result;
}

/// How many items icacls admitted it could not do. It prints this even in the runs where
/// it goes on to return zero, which is the only reason the number is worth reading.
int failedCount(const Run &r)
{
    static const QRegularExpression pattern(QStringLiteral("Failed processing (\\d+)"));
    const QRegularExpressionMatch match = pattern.match(r.output);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

QString describe(const QString &step, const Run &r)
{
    return QStringLiteral("$ %1  → %2 %3\n%4")
        .arg(step, Locale::tr(QStringLiteral("own.exitCode")))
        .arg(r.code)
        .arg(r.output);
}

} // namespace

Ownership::Result Ownership::take(const QString &path)
{
    Result result;
    const QFileInfo info(path);
    if (!info.exists()) {
        result.summary = Locale::tr(QStringLiteral("own.notFound")).arg(QDir::toNativeSeparators(path));
        return result;
    }

    const QString native = QDir::toNativeSeparators(path);
    const bool folder = info.isDir();
    const QString user = currentUserSid();

    // Ownership goes to the signed-in account rather than to the Administrators group.
    // Granting the group instead is the mistake that makes this feel like it did not
    // work: an unelevated Explorer carries Administrators as a deny-only SID, so the
    // file stays untouchable in the very window the user is standing in.
    QStringList ownArgs{QStringLiteral("/f"), native};
    if (folder)
        ownArgs << QStringLiteral("/r") << QStringLiteral("/d") << QStringLiteral("y");
    const Run owned = run(QStringLiteral("takeown.exe"), ownArgs);

    // Both principals get full control: the user so the file is usable without another
    // prompt, and Administrators so any other admin account can still repair it.
    // /grant:r replaces what those two had rather than adding to it, so a deny ACE left
    // behind by an installer does not survive.
    QStringList aclArgs{native, QStringLiteral("/grant:r")};
    if (!user.isEmpty())
        aclArgs << QStringLiteral("*%1:F").arg(user);
    aclArgs << QStringLiteral("*%1:F").arg(AdminsSid);
    if (folder)
        aclArgs << QStringLiteral("/t") << QStringLiteral("/c") << QStringLiteral("/l")
                << QStringLiteral("/q");
    const Run granted = run(QStringLiteral("icacls.exe"), aclArgs);

    result.detail = describe(QStringLiteral("takeown"), owned) + QStringLiteral("\n\n")
                    + describe(QStringLiteral("icacls"), granted);

    // The owner read back from the file is what decides this, not the exit codes: with
    // /c in play icacls returns zero even for a file it never opened.
    const QString nowOwned = ownerSid(path);
    const bool mine = !nowOwned.isEmpty() && (nowOwned == user || nowOwned == AdminsSid);
    result.ok = mine && failedCount(granted) == 0;

    if (result.ok)
        result.summary = Locale::tr(QStringLiteral("own.ok")).arg(native);
    else if (mine)
        result.summary = Locale::tr(QStringLiteral("own.partial")).arg(native);
    else
        result.summary = Locale::tr(QStringLiteral("own.failed")).arg(native);
    return result;
}

Ownership::Result Ownership::giveBack(const QString &path)
{
    Result result;
    const QFileInfo info(path);
    if (!info.exists()) {
        result.summary = Locale::tr(QStringLiteral("own.notFound")).arg(QDir::toNativeSeparators(path));
        return result;
    }

    const QString native = QDir::toNativeSeparators(path);
    const bool folder = info.isDir();

    QStringList args{native, QStringLiteral("/setowner"),
                     QStringLiteral("*%1").arg(TrustedInstallerSid)};
    if (folder)
        args << QStringLiteral("/t") << QStringLiteral("/c") << QStringLiteral("/l")
             << QStringLiteral("/q");
    const Run restored = run(QStringLiteral("icacls.exe"), args);

    result.detail = describe(QStringLiteral("icacls /setowner"), restored);
    result.ok = ownerSid(path) == TrustedInstallerSid;
    result.summary = result.ok
                         ? QStringLiteral("Sahiplik TrustedInstaller'a verildi: %1").arg(native)
                         : QStringLiteral("Sahiplik geri verilemedi: %1").arg(native);
    return result;
}
