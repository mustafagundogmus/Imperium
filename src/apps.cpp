#include "apps.h"
#include "console.h"
#include "winpaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>

namespace Apps {

namespace {

bool carriesId(const QString &id)
{
    const QString t = id.trimmed();
    return !t.isEmpty() && t.compare(QLatin1String("na"), Qt::CaseInsensitive) != 0;
}

/// WinUtil's functions, from functions/private/, one after another. Only Write-WinUtilLog
/// is this file's own: WinUtil's writes to its log file, this one hands the same line back
/// to the page as a token. The console-output banners are kept — they are what the log
/// panel shows, and a user who has run WinUtil will recognise them.
const char *const Preamble = R"PS(
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
# winget lays its table out to the console width and elides an id that does not fit; the
# hidden console this runs in is 80 columns wide, which is narrow enough to lose the tail
# of "Microsoft.VisualStudioCode.Insiders". Widened before anything prints.
try {
    $raw = $Host.UI.RawUI
    $size = $raw.BufferSize
    if ($size.Width -lt 512) { $size.Width = 512; $raw.BufferSize = $size }
} catch {}

function Write-WinUtilLog {
    param(
        [string]$Component = "General",
        [string]$Message = "",
        [string]$Level = "INFO"
    )
    Write-Output "ARB-LOG|$Level|$Component|$Message"
}

function Test-WinUtilPackageManager {
    <#

    .SYNOPSIS
        Checks if WinGet and/or Choco are installed

    .PARAMETER winget
        Check if WinGet is installed

    .PARAMETER choco
        Check if Chocolatey is installed

    #>

    Param(
        [System.Management.Automation.SwitchParameter]$winget,
        [System.Management.Automation.SwitchParameter]$choco
    )

    if ($winget) {
        if (Get-Command winget -ErrorAction SilentlyContinue) {
            Write-Host "===========================================" -ForegroundColor Green
            Write-Host "---        WinGet is installed          ---" -ForegroundColor Green
            Write-Host "===========================================" -ForegroundColor Green
            $status = "installed"
        } else {
            Write-Host "===========================================" -ForegroundColor Red
            Write-Host "---      WinGet is not installed        ---" -ForegroundColor Red
            Write-Host "===========================================" -ForegroundColor Red
            $status = "not-installed"
        }
    }

    if ($choco) {
        if (Get-Command choco -ErrorAction SilentlyContinue) {
            Write-Host "===========================================" -ForegroundColor Green
            Write-Host "---      Chocolatey is installed        ---" -ForegroundColor Green
            Write-Host "===========================================" -ForegroundColor Green
            $status = "installed"
        } else {
            Write-Host "===========================================" -ForegroundColor Red
            Write-Host "---    Chocolatey is not installed      ---" -ForegroundColor Red
            Write-Host "===========================================" -ForegroundColor Red
            $status = "not-installed"
        }
    }

    return $status
}

function Install-WinUtilWinget {
    <#

    .SYNOPSIS
        Installs WinGet if not already installed.

    .DESCRIPTION
        installs winGet if needed
    #>
    if ((Test-WinUtilPackageManager -winget) -eq "installed") {
        return
    }

    Write-Host "WinGet is not installed. Installing now..." -ForegroundColor Red

    Install-PackageProvider -Name NuGet -Force
    Install-Module -Name Microsoft.WinGet.Client -Force
    Repair-WinGetPackageManager -AllUsers
}

function Install-WinUtilChoco {
    if (-not (Get-Command -Name choco)) {
      Write-Host "Chocolatey is not installed. Installing now..."
      $installScript = Invoke-WebRequest -Uri https://community.chocolatey.org/install.ps1 -UseBasicParsing
      Invoke-Command -ScriptBlock ([scriptblock]::Create($installScript.Content))
    }
}

Function Install-WinUtilProgramWinget {
    param (
        [Parameter(Mandatory=$true)]
        [ValidateSet("Install", "Uninstall")]
        [string]$Action,

        [Parameter(Mandatory=$true)]
        [string[]]$Programs
    )

    foreach ($program in $Programs) {
        if ([string]::IsNullOrWhiteSpace($program) -or $program -eq "na") {
            continue
        }

        $source = "winget"
        if ($program.StartsWith("msstore:", [System.StringComparison]::OrdinalIgnoreCase)) {
            $source = "msstore"
            $program = $program.Substring("msstore:".Length)
        }

        if ($Action -eq 'Install') {
            $arguments = @("install", "--id", $program, "--accept-package-agreements", "--accept-source-agreements", "--source", $source, "--silent")
        } else {
            $arguments = @("uninstall", "--id", $program, "--source", $source, "--silent")
        }

        Write-WinUtilLog -Component "Package" -Message "$Action winget package: $program (source: $source)"
        $process = Start-Process -FilePath winget -ArgumentList $arguments -NoNewWindow -Wait -PassThru
        Write-WinUtilLog -Component "Package" -Message "$Action winget package completed: $program (exit code: $($process.ExitCode))"
    }
}

function Install-WinUtilProgramChoco {
    param (
        [Parameter(Mandatory=$true)]
        [ValidateSet("Install", "Uninstall")]
        [string]$Action,

        [Parameter(Mandatory=$true)]
        [string[]]$Programs
    )

    if ($Action -eq 'Install') {
        $arguments = "install $Programs -y"
    } else {
        $arguments = "uninstall $Programs -y"
    }

    Write-WinUtilLog -Component "Package" -Message "$Action choco package(s): $($Programs -join ', ')"
    $process = Start-Process -FilePath choco -ArgumentList $arguments -NoNewWindow -Wait -PassThru
    Write-WinUtilLog -Component "Package" -Message "$Action choco package(s) completed: $($Programs -join ', ') (exit code: $($process.ExitCode))"
}
)PS";

QString psQuote(const QString &s)
{
    return QLatin1Char('\'') + QString(s).replace(QLatin1Char('\''), QStringLiteral("''"))
           + QLatin1Char('\'');
}

QString psArray(const QStringList &ids)
{
    QStringList quoted;
    for (const QString &id : ids)
        quoted << psQuote(id);
    return QStringLiteral("@(") + quoted.join(QStringLiteral(", ")) + QLatin1Char(')');
}

/// Invoke-WPFInstall's and Invoke-WPFUnInstall's runspace body, the progress-indicator
/// calls turned into tokens. \a action is Install or Uninstall.
QString batchBody(const Split &s, Operation op)
{
    const bool install = op == Operation::Install;
    const QString action = install ? QStringLiteral("Install") : QStringLiteral("Uninstall");

    QString body;
    body += QStringLiteral("$packagesWinget = %1\n").arg(psArray(s.winget));
    body += QStringLiteral("$packagesChoco = %1\n").arg(psArray(s.choco));
    body += QStringLiteral("$totalPackages = @($packagesWinget).Count + @($packagesChoco).Count\n"
                           "$completedPackages = 0\n");
    body += QStringLiteral("Write-WinUtilLog -Component \"%1\" -Message \"%1 package manager split: "
                           "winget=$(@($packagesWinget).Count), choco=$(@($packagesChoco).Count)\"\n")
                .arg(action);
    body += QStringLiteral("try {\n");

    if (!install) {
        // Verbatim from Invoke-WPFUnInstall: Edge's uninstaller refuses to run when the
        // legacy shell app's executable is missing, so WinUtil puts an empty one there.
        body += QStringLiteral(
            "    if ($packagesWinget -contains \"Microsoft.Edge\") {\n"
            "        New-Item -Path \"$Env:SystemRoot\\SystemApps\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\\MicrosoftEdge.exe\" -Force\n"
            "    }\n");
    }

    body += QStringLiteral(
        "    if ($packagesWinget.Count -gt 0 -and $packagesWinget -ne \"0\") {\n");
    if (install)
        body += QStringLiteral("        Install-WinUtilWinget\n");
    body += QStringLiteral(
        "        foreach ($program in $packagesWinget) {\n"
        "            $position = $completedPackages + 1\n"
        "            Write-Output \"ARB-APP|start|$position|$totalPackages|$program\"\n"
        "            Install-WinUtilProgramWinget -Action %1 -Programs @($program)\n"
        "            $completedPackages++\n"
        "            Write-Output \"ARB-APP|done|$completedPackages|$totalPackages|$program\"\n"
        "        }\n"
        "    }\n").arg(action);
    body += QStringLiteral(
        "    if ($packagesChoco.Count -gt 0) {\n"
        "        $position = $completedPackages + 1\n"
        "        Write-Output \"ARB-APP|start|$position|$totalPackages|choco|$($packagesChoco -join ',')\"\n");
    if (install)
        body += QStringLiteral("        Install-WinUtilChoco\n");
    body += QStringLiteral(
        "        Install-WinUtilProgramChoco -Action %1 -Programs $packagesChoco\n"
        "        $completedPackages += @($packagesChoco).Count\n"
        "        Write-Output \"ARB-APP|done|$completedPackages|$totalPackages|choco|$($packagesChoco -join ',')\"\n"
        "    }\n").arg(action);

    const QString banner = install ? QStringLiteral("--      Installs have finished          ---")
                                   : QStringLiteral("--       Uninstalls have finished       ---");
    body += QStringLiteral(
        "    Write-Host \"===========================================\"\n"
        "    Write-Host \"%1\"\n"
        "    Write-Host \"===========================================\"\n"
        "    Write-WinUtilLog -Component \"%2\" -Message \"%2 workflow completed.\"\n"
        "    Write-Output \"ARB-APP|end|$completedPackages|$totalPackages\"\n"
        "} catch {\n"
        "    Write-Host \"===========================================\"\n"
        "    Write-Host \"Error: $_\"\n"
        "    Write-Host \"===========================================\"\n"
        "    Write-WinUtilLog -Level \"ERROR\" -Component \"%2\" -Message \"%2 workflow failed: $($_.Exception.Message)\"\n"
        "    exit 1\n"
        "}\n").arg(banner, action);
    return body;
}

/// Invoke-WinUtilCurrentSystem's two listing branches. The matching against the catalogue
/// happens in C++ (see Runner::onFinished) with the same patterns.
QString detectBody(Manager m)
{
    QString body;
    body += QStringLiteral("$originalEncoding = [Console]::OutputEncoding\n"
                           "try {\n"
                           "    [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()\n"
                           "    Write-Output \"ARB-LIST-BEGIN\"\n");
    if (m == Manager::Winget) {
        body += QStringLiteral(
            "    $installedProgramOutput = @(winget list --accept-source-agreements --disable-interactivity 2>&1)\n"
            "    if ($LASTEXITCODE -ne 0) {\n"
            "        Write-Output \"ARB-LIST-ERROR|winget list failed with exit code $LASTEXITCODE.\"\n"
            "    } else {\n"
            "        $installedProgramOutput | ForEach-Object { Write-Output ([string]$_) }\n"
            "    }\n");
    } else {
        body += QStringLiteral(
            "    $apps = (choco list | Select-String -Pattern \"^\\S+\").Matches.Value\n"
            "    $apps | ForEach-Object { Write-Output ([string]$_) }\n");
    }
    body += QStringLiteral("    Write-Output \"ARB-LIST-END\"\n"
                           "} finally {\n"
                           "    [Console]::OutputEncoding = $originalEncoding\n"
                           "}\n");
    return body;
}

QString repairBody(Manager m)
{
    if (m == Manager::Winget) {
        // Invoke-WPFFixesWinget.
        return QStringLiteral(
            "try {\n"
            "    Write-Host \"==> Starting WinGet Repair\"\n"
            "    Install-WinUtilWinget\n"
            "} catch {\n"
            "    Write-Error \"Failed to install WinGet: $_\"\n"
            "} finally {\n"
            "    Write-Host \"==> Finished WinGet Repair\"\n"
            "}\n"
            "$status = Test-WinUtilPackageManager -winget\n"
            "Write-Output \"ARB-PM|winget|$status\"\n"
            "if ($status -ne 'installed') { exit 1 }\n");
    }
    return QStringLiteral(
        "try {\n"
        "    Install-WinUtilChoco\n"
        "} catch {\n"
        "    Write-Error \"Failed to install Chocolatey: $_\"\n"
        "}\n"
        "$status = Test-WinUtilPackageManager -choco\n"
        "Write-Output \"ARB-PM|choco|$status\"\n"
        "if ($status -ne 'installed') { exit 1 }\n");
}

QString probeBody()
{
    return QStringLiteral("$w = Test-WinUtilPackageManager -winget\n"
                          "Write-Output \"ARB-PM|winget|$w\"\n"
                          "$c = Test-WinUtilPackageManager -choco\n"
                          "Write-Output \"ARB-PM|choco|$c\"\n");
}

/// Invoke-WPFInstallUpgrade, for the window of its own.
QString upgradeBody(Manager m)
{
    QString body;
    body += m == Manager::Choco ? QStringLiteral("Install-WinUtilChoco\n")
                                : QStringLiteral("Install-WinUtilWinget\n");
    body += QStringLiteral(
        "Write-Host \"===========================================\"\n"
        "Write-Host \"--           Updates started            ---\"\n"
        "Write-Host \"-- You can close this window if desired ---\"\n"
        "Write-Host \"===========================================\"\n");
    body += m == Manager::Choco
                ? QStringLiteral("choco upgrade all -y\n")
                : QStringLiteral("winget upgrade --all --include-unknown --silent "
                                 "--accept-source-agreements --accept-package-agreements\n");
    return body;
}

/// The exit codes winget answers with when there was nothing to do: the package is
/// already installed at the newest version (UPDATE_NOT_APPLICABLE, 0x8A15002B) or the
/// same version is already there (PACKAGE_ALREADY_INSTALLED, 0x8A150061). WinUtil logs
/// the code and moves on; a row here says "already current" rather than "failed".
bool alreadyCurrent(int exitCode)
{
    return exitCode == -1978335189 || exitCode == -1978335135;
}

} // namespace

// ------------------------------------------------------------------ Entry ---

bool Entry::hasWinget() const
{
    return carriesId(winget);
}

bool Entry::hasChoco() const
{
    return carriesId(choco);
}

QString categorySlug(const QString &category)
{
    QString slug;
    for (const QChar &c : category) {
        if (c.isLetterOrNumber())
            slug += c.toLower();
        else if (!slug.isEmpty() && !slug.endsWith(QLatin1Char('-')))
            slug += QLatin1Char('-');
    }
    while (slug.endsWith(QLatin1Char('-')))
        slug.chop(1);
    return slug;
}

QString categoryKey(const QString &category)
{
    return QStringLiteral("apps.category.") + categorySlug(category);
}

// -------------------------------------------------------------- Catalogue ---

const Catalogue &Catalogue::instance()
{
    static const Catalogue c;
    return c;
}

Catalogue::Catalogue()
{
    QFile file(QStringLiteral(":/data/applications.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("applications.json: cannot open resource");
        return;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning("applications.json: %s", qUtf8Printable(error.errorString()));
        return;
    }

    const QJsonObject root = doc.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        if (it.key().startsWith(QLatin1Char('_')) || !it.value().isObject())
            continue;
        const QJsonObject o = it.value().toObject();

        Entry e;
        e.key = it.key();
        e.category = o.value(QStringLiteral("category")).toString().trimmed();
        e.name = o.value(QStringLiteral("content")).toString().trimmed();
        e.description = o.value(QStringLiteral("description")).toString().trimmed();
        e.link = o.value(QStringLiteral("link")).toString().trimmed();
        e.winget = o.value(QStringLiteral("winget")).toString().trimmed();
        e.choco = o.value(QStringLiteral("choco")).toString().trimmed();
        e.foss = o.value(QStringLiteral("foss")).toBool();
        if (e.name.isEmpty())
            e.name = e.key;
        if (e.category.isEmpty())
            e.category = QStringLiteral("Utilities");
        m_entries.append(e);
    }

    // Initialize-InstallCategoryAppList: categories sorted, keys sorted within each.
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) {
        const int c = a.category.compare(b.category, Qt::CaseInsensitive);
        if (c != 0)
            return c < 0;
        return a.key.compare(b.key, Qt::CaseInsensitive) < 0;
    });

    for (int i = 0; i < m_entries.size(); ++i) {
        m_index.insert(m_entries.at(i).key, i);
        if (m_categories.isEmpty() || m_categories.last() != m_entries.at(i).category)
            m_categories.append(m_entries.at(i).category);
    }
}

const Entry *Catalogue::entry(const QString &key) const
{
    const int i = m_index.value(key, -1);
    return i >= 0 ? &m_entries.at(i) : nullptr;
}

QString Catalogue::keyFromPresetToken(const QString &token) const
{
    QString t = token.trimmed();
    if (t.startsWith(QLatin1String("WPFInstall"), Qt::CaseInsensitive))
        t = t.mid(int(QStringLiteral("WPFInstall").size()));
    if (m_index.contains(t))
        return t;
    // WinUtil sanitises a key with a dash for WPF ("es-de" becomes "es_de"); accept the
    // sanitised spelling back.
    const QString unsanitised = QString(t).replace(QLatin1Char('_'), QLatin1Char('-'));
    if (m_index.contains(unsanitised))
        return unsanitised;
    // Case-insensitive last resort: a hand-edited file.
    for (auto it = m_index.constBegin(); it != m_index.constEnd(); ++it)
        if (it.key().compare(t, Qt::CaseInsensitive) == 0)
            return it.key();
    return QString();
}

// ---------------------------------------------------------------- Manager ---

QString managerToString(Manager m)
{
    return m == Manager::Choco ? QStringLiteral("Choco") : QStringLiteral("Winget");
}

Manager managerFromString(const QString &s)
{
    return s.compare(QLatin1String("Choco"), Qt::CaseInsensitive) == 0 ? Manager::Choco
                                                                        : Manager::Winget;
}

Split split(const QVector<Entry> &packages, Manager preference)
{
    // Get-WinUtilSelectedPackages, including its Add-PackageId rules.
    Split s;
    const auto add = [](QStringList &target, const QString &id) {
        if (!carriesId(id))
            return;
        if (!target.contains(id))
            target.append(id);
    };
    for (const Entry &e : packages) {
        switch (preference) {
        case Manager::Choco:
            if (!carriesId(e.choco))
                add(s.winget, e.winget);
            else
                add(s.choco, e.choco);
            break;
        case Manager::Winget:
            add(s.winget, e.winget);
            break;
        }
    }
    return s;
}

QString commandSummary(const Split &s, Operation op)
{
    QStringList lines;
    for (const QString &raw : s.winget) {
        QString id = raw;
        QString source = QStringLiteral("winget");
        if (id.startsWith(QLatin1String("msstore:"), Qt::CaseInsensitive)) {
            source = QStringLiteral("msstore");
            id = id.mid(int(QStringLiteral("msstore:").size()));
        }
        if (op == Operation::Install)
            lines << QStringLiteral("winget install --id %1 --accept-package-agreements "
                                    "--accept-source-agreements --source %2 --silent")
                         .arg(id, source);
        else
            lines << QStringLiteral("winget uninstall --id %1 --source %2 --silent").arg(id, source);
    }
    if (!s.choco.isEmpty())
        lines << QStringLiteral("choco %1 %2 -y")
                     .arg(op == Operation::Install ? QStringLiteral("install")
                                                   : QStringLiteral("uninstall"),
                          s.choco.join(QLatin1Char(' ')));
    return lines.join(QLatin1Char('\n'));
}

// ------------------------------------------------------- import / export ---

bool exportSelection(const QString &path, const QStringList &keys, QString *error)
{
    QJsonArray array;
    for (const QString &key : keys)
        array.append(QStringLiteral("WPFInstall") + key);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return true;
}

bool importSelection(const QString &path, QStringList *keys, int *unknown, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError) {
        if (error)
            *error = parse.errorString();
        return false;
    }

    QStringList tokens;
    if (doc.isArray()) {
        // The current format: a flat array of every checkbox key. Only the install ones
        // are this page's business; the tweak, feature and toggle keys pass through.
        for (const QJsonValue &v : doc.array())
            if (v.isString() && v.toString().startsWith(QLatin1String("WPFInstall"),
                                                        Qt::CaseInsensitive))
                tokens << v.toString();
    } else if (doc.isObject()) {
        // The legacy export: keys grouped under their family name.
        const QJsonValue installs = doc.object().value(QStringLiteral("WPFInstall"));
        for (const QJsonValue &v : installs.toArray())
            if (v.isString())
                tokens << v.toString();
    }

    const Catalogue &catalogue = Catalogue::instance();
    QStringList found;
    int missing = 0;
    for (const QString &token : std::as_const(tokens)) {
        const QString key = catalogue.keyFromPresetToken(token);
        if (key.isEmpty())
            ++missing;
        else if (!found.contains(key))
            found << key;
    }
    if (keys)
        *keys = found;
    if (unknown)
        *unknown = missing;
    return true;
}

// ----------------------------------------------------------------- Runner ---

Runner::Runner(QObject *parent)
    : QObject(parent)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_scriptPath = dir + QStringLiteral("/apps.ps1");
    m_logPath = dir + QStringLiteral("/apps.log");
}

void Runner::install(const Split &s)
{
    start(Job::Install, batchBody(s, Operation::Install), s.total());
}

void Runner::uninstall(const Split &s)
{
    start(Job::Uninstall, batchBody(s, Operation::Uninstall), s.total());
}

void Runner::detectInstalled(Manager m)
{
    m_detectManager = m;
    start(Job::Detect, detectBody(m), 0);
}

void Runner::repair(Manager m)
{
    start(Job::Repair, repairBody(m), 0);
}

void Runner::probe()
{
    start(Job::Probe, probeBody(), 0);
}

bool Runner::upgradeAll(Manager m, QString *error)
{
    const QString powershell = WinPaths::powershell();
    if (powershell.isEmpty()) {
        if (error)
            *error = QStringLiteral("powershell");
        return false;
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString path = dir + QStringLiteral("/apps-upgrade.ps1");
    QFile script(path);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = script.errorString();
        return false;
    }
    script.write("\xEF\xBB\xBF");
    // Without the token stub: this window is the user's own, and Write-WinUtilLog there
    // should read as a sentence, not as a token nobody parses.
    QString body = QString::fromUtf8(Preamble);
    body.replace(QStringLiteral("Write-Output \"ARB-LOG|$Level|$Component|$Message\""),
                 QStringLiteral("Write-Host \"[$Level] [$Component] $Message\""));
    script.write(body.toUtf8());
    script.write(upgradeBody(m).toUtf8());
    script.close();

    // startDetached opens a console of its own (CREATE_NEW_CONSOLE), which is exactly
    // the "you can close this window if desired" WinUtil prints into it.
    const bool ok = QProcess::startDetached(
        powershell,
        {QStringLiteral("-NoExit"), QStringLiteral("-NoProfile"),
         QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
         QStringLiteral("-File"), QDir::toNativeSeparators(path)});
    if (!ok && error)
        *error = QStringLiteral("start");
    return ok;
}

void Runner::start(Job job, const QString &body, int total)
{
    if (m_process)
        return;

    const QString powershell = WinPaths::powershell();
    if (powershell.isEmpty()) {
        m_job = job;
        Q_EMIT started(job, total);
        m_job = Job::None;
        Q_EMIT finished(job, false, 0);
        return;
    }

    QFile script(m_scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_job = job;
        Q_EMIT started(job, total);
        m_job = Job::None;
        Q_EMIT finished(job, false, 0);
        return;
    }
    script.write("\xEF\xBB\xBF");
    script.write(Preamble);
    script.write("\n");
    script.write(body.toUtf8());
    script.write("\n");
    script.close();

    m_job = job;
    m_total = total;
    m_done = 0;
    m_failures = 0;
    m_lastExitCode = 0;
    m_listing = false;
    m_wingetPresent = false;
    m_chocoPresent = false;
    m_carry.clear();
    m_listText.clear();
    m_lastPackage.clear();
    m_transcript.clear();

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setProgram(powershell);
    m_process->setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                             QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                             QStringLiteral("-File"), QDir::toNativeSeparators(m_scriptPath)});

    connect(m_process, &QProcess::readyReadStandardOutput, this, &Runner::consume);
    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus) { onFinished(exitCode); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process)
            onFinished(-1);
    });

    // Before start(), for the reason ActionEngine gives: a launch failure reports
    // synchronously, and finished() must not reach the page before started() did.
    Q_EMIT started(job, total);
    m_process->start();
}

void Runner::consume()
{
    if (!m_process)
        return;
    const QByteArray raw = m_process->readAllStandardOutput();
    if (raw.isEmpty())
        return;

    // The script pins UTF-8, so that is the first reading; a chunk that is not valid
    // UTF-8 (a tool that ignored the console code page) falls back to the console's own.
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter))
        text = Console::decode(raw);

    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));   // winget's progress rewrites
    text.prepend(m_carry);
    m_carry.clear();

    int from = 0;
    while (true) {
        const int nl = text.indexOf(QLatin1Char('\n'), from);
        if (nl < 0) {
            m_carry = text.mid(from);
            break;
        }
        handleLine(text.mid(from, nl - from));
        from = nl + 1;
    }
}

void Runner::handleLine(const QString &raw)
{
    const QString text = raw.trimmed();
    if (text.isEmpty())
        return;

    m_transcript += text + QLatin1Char('\n');

    if (text == QLatin1String("ARB-LIST-BEGIN")) {
        m_listing = true;
        return;
    }
    if (text == QLatin1String("ARB-LIST-END")) {
        m_listing = false;
        return;
    }
    if (text.startsWith(QLatin1String("ARB-LIST-ERROR|"))) {
        m_listing = false;
        ++m_failures;
        Q_EMIT line(text.mid(int(QStringLiteral("ARB-LIST-ERROR|").size())));
        return;
    }
    if (m_listing) {
        m_listText += text + QLatin1Char('\n');
        return;
    }

    if (text.startsWith(QLatin1String("ARB-APP|"))) {
        const QStringList f = text.split(QLatin1Char('|'));
        const QString kind = f.value(1);
        if (kind == QLatin1String("start")) {
            m_lastPackage = f.value(4) == QLatin1String("choco") ? f.value(5) : f.value(4);
            Q_EMIT progress(m_done, m_total, m_lastPackage, false, 0);
        } else if (kind == QLatin1String("done")) {
            m_done = f.value(2).toInt();
            const QString id = f.value(4) == QLatin1String("choco") ? f.value(5) : f.value(4);
            const int code = m_lastExitCode;
            m_lastExitCode = 0;
            if (code != 0 && !alreadyCurrent(code))
                ++m_failures;
            Q_EMIT progress(m_done, m_total, id, true, code);
        }
        return;
    }

    if (text.startsWith(QLatin1String("ARB-LOG|"))) {
        const QStringList f = text.split(QLatin1Char('|'));
        const QString message = f.mid(3).join(QLatin1Char('|'));
        static const QRegularExpression exitRe(QStringLiteral("exit code: (-?\\d+)"));
        const QRegularExpressionMatch m = exitRe.match(message);
        if (m.hasMatch())
            m_lastExitCode = m.captured(1).toInt();
        Q_EMIT line(QStringLiteral("[%1] %2").arg(f.value(2), message));
        return;
    }

    if (text.startsWith(QLatin1String("ARB-PM|"))) {
        const QStringList f = text.split(QLatin1Char('|'));
        const bool present = f.value(2) == QLatin1String("installed");
        if (f.value(1) == QLatin1String("winget"))
            m_wingetPresent = present;
        else if (f.value(1) == QLatin1String("choco"))
            m_chocoPresent = present;
        return;
    }

    Q_EMIT line(text);
}

void Runner::onFinished(int exitCode)
{
    if (!m_process)
        return;

    consume();
    if (!m_carry.isEmpty()) {
        handleLine(m_carry);
        m_carry.clear();
    }

    QFile log(m_logPath);
    if (log.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&log);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << "  "
            << QString::fromLatin1(QMetaEnum::fromType<Job>().valueToKey(int(m_job)))
            << "  exit=" << exitCode << '\n';
        if (!m_transcript.isEmpty())
            out << m_transcript;
        out << "---\n";
    }

    const Job job = m_job;
    m_job = Job::None;
    m_process->deleteLater();
    m_process = nullptr;

    bool ok = exitCode == 0;

    if (job == Job::Detect) {
        // Invoke-WinUtilCurrentSystem's matching, entry by entry.
        QStringList keys;
        if (ok && m_failures == 0) {
            const Catalogue &catalogue = Catalogue::instance();
            if (m_detectManager == Manager::Winget) {
                for (const Entry &e : catalogue.entries()) {
                    QString id = e.winget.section(QLatin1Char(';'), -1);
                    id.remove(QRegularExpression(QStringLiteral("^msstore:")));
                    id = id.trimmed();
                    if (!carriesId(id))
                        continue;
                    const QRegularExpression pattern(
                        QStringLiteral("[^\\S\\r\\n]{2,}%1(?=[^\\S\\r\\n]{2,}|$)")
                            .arg(QRegularExpression::escape(id)),
                        QRegularExpression::CaseInsensitiveOption
                            | QRegularExpression::MultilineOption);
                    if (pattern.match(m_listText).hasMatch())
                        keys << e.key;
                }
            } else {
                QSet<QString> apps;
                for (const QString &l : m_listText.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
                    apps.insert(l.section(QLatin1Char(' '), 0, 0).toLower());
                for (const Entry &e : catalogue.entries()) {
                    const QString id = e.choco.section(QLatin1Char(';'), -1).trimmed();
                    if (carriesId(id) && apps.contains(id.toLower()))
                        keys << e.key;
                }
            }
        } else {
            ok = false;
        }
        Q_EMIT detected(keys);
    }

    if (job == Job::Probe)
        Q_EMIT probed(m_wingetPresent, m_chocoPresent);

    Q_EMIT finished(job, ok, m_failures);
}

} // namespace Apps
