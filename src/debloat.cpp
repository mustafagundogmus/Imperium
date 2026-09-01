#include "debloat.h"
#include "winpaths.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QXmlStreamReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {

// The only hard-coded package names in this file, and they exist to stop a removal, never
// to drive the list. These are shared runtimes: they are not applications at all, they are
// the libraries other installed apps link against, so removing one silently breaks every
// unrelated app that depends on it rather than the thing you meant to uninstall. Windows
// reports them as ordinary removable packages because nothing in the package model says
// "something else needs this". Matched by prefix, since each runtime ships one package per
// major version (…Main.1.8, …CBS.1.6, and so on).
//
// Everything else the user may take off, including the Store itself. Anything genuinely
// load-bearing beyond these already carries Windows' own NonRemovable flag or a System
// signature, which the scan honours automatically.
const QStringList &criticalPrefixes()
{
    static const QStringList prefixes = {
        QStringLiteral("Microsoft.WindowsAppRuntime"),
        QStringLiteral("MicrosoftCorporationII.WinAppRuntime"),
        QStringLiteral("Microsoft.VCLibs"),
        QStringLiteral("Microsoft.NET.Native"),
        QStringLiteral("Microsoft.UI.Xaml"),
    };
    return prefixes;
}

bool isCritical(const QString &packageName)
{
    for (const QString &prefix : criticalPrefixes())
        if (packageName.startsWith(prefix, Qt::CaseInsensitive))
            return true;
    return false;
}

/// Windows names a manifest's referenced logo as a base name that never exists on disk
/// directly — the real files carry a resource-qualifier suffix
/// (`.scale-200.png`, `.targetsize-256.png`, optionally `_altform-unplated`). This picks
/// the largest plain variant it can find, falling back to an altform one rather than
/// nothing.
QString bestLogoCandidate(const QString &dir, const QString &baseName)
{
    const QString exact = dir + QLatin1Char('/') + baseName;
    if (QFile::exists(exact))
        return exact;

    const int dot = baseName.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot >= 0 ? baseName.left(dot) : baseName;
    const QString ext = dot >= 0 ? baseName.mid(dot) : QString();

    QDir d(dir);
    if (!d.exists())
        return QString();
    const QStringList all = d.entryList(QDir::Files);

    static const QRegularExpression sizeRe(
        QStringLiteral("\\.(?:scale|targetsize)-(\\d+)"),
        QRegularExpression::CaseInsensitiveOption);

    QString bestPlain, bestAltform;
    int bestPlainScore = -1, bestAltformScore = -1;

    for (const QString &f : all) {
        if (!f.startsWith(stem, Qt::CaseInsensitive) || !f.endsWith(ext, Qt::CaseInsensitive))
            continue;

        const bool altform = f.contains(QStringLiteral("_altform"), Qt::CaseInsensitive);
        const QRegularExpressionMatch m = sizeRe.match(f);
        const int score = m.hasMatch() ? m.captured(1).toInt() : 0;

        if (altform) {
            if (score > bestAltformScore) {
                bestAltformScore = score;
                bestAltform = f;
            }
        } else if (score > bestPlainScore) {
            bestPlainScore = score;
            bestPlain = f;
        }
    }

    const QString chosen = !bestPlain.isEmpty() ? bestPlain : bestAltform;
    return chosen.isEmpty() ? QString() : dir + QLatin1Char('/') + chosen;
}

struct ManifestInfo
{
    QString displayName;
    QString publisher;
    QPixmap logo;
};

/// Reads what the package says about itself. A DisplayName of the form `ms-resource:…` is
/// an indirection into the package's compiled resource table, which cannot be read without
/// loading the package's own resource map — those are left empty here and the Start-menu
/// name (which Windows has already resolved and localised) is used instead.
ManifestInfo readManifest(const QString &installLocation)
{
    ManifestInfo info;
    if (installLocation.isEmpty())
        return info;

    QFile manifest(installLocation + QStringLiteral("/AppxManifest.xml"));
    if (!manifest.open(QIODevice::ReadOnly))
        return info;

    QXmlStreamReader xml(&manifest);

    QString defaultLogo, fallbackLogo, propertiesLogo;
    bool inProperties = false;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.tokenType() == QXmlStreamReader::EndElement
            && xml.name() == QLatin1String("Properties")) {
            inProperties = false;
            continue;
        }
        if (xml.tokenType() != QXmlStreamReader::StartElement)
            continue;

        // QXmlStreamReader::name() is the local name — the manifest's uap: prefix does
        // not need to be matched here.
        const QStringView name = xml.name();

        if (name == QLatin1String("Properties")) {
            inProperties = true;
        } else if (inProperties && name == QLatin1String("DisplayName")) {
            const QString text = xml.readElementText();
            if (!text.startsWith(QLatin1String("ms-resource:")))
                info.displayName = text;
        } else if (inProperties && name == QLatin1String("PublisherDisplayName")) {
            const QString text = xml.readElementText();
            if (!text.startsWith(QLatin1String("ms-resource:")))
                info.publisher = text;
        } else if (inProperties && name == QLatin1String("Logo")) {
            propertiesLogo = xml.readElementText();
        } else if (name == QLatin1String("VisualElements")) {
            const auto &attrs = xml.attributes();
            const QString appListEntry = attrs.value(QStringLiteral("AppListEntry")).toString();
            if (appListEntry.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
                continue;   // a background task / CLI entry, not the app itself

            QString logo = attrs.value(QStringLiteral("Square44x44Logo")).toString();
            if (logo.isEmpty())
                logo = attrs.value(QStringLiteral("Square150x150Logo")).toString();
            if (logo.isEmpty())
                continue;

            if (fallbackLogo.isEmpty())
                fallbackLogo = logo;
            if (defaultLogo.isEmpty()
                || appListEntry.compare(QLatin1String("default"), Qt::CaseInsensitive) == 0)
                defaultLogo = logo;
        }
    }

    QString chosen = !defaultLogo.isEmpty()    ? defaultLogo
                     : !fallbackLogo.isEmpty() ? fallbackLogo
                                               : propertiesLogo;
    if (chosen.isEmpty())
        return info;

    chosen.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int slash = chosen.lastIndexOf(QLatin1Char('/'));
    const QString relDir = slash >= 0 ? chosen.left(slash) : QString();
    const QString baseName = slash >= 0 ? chosen.mid(slash + 1) : chosen;
    const QString dir =
        relDir.isEmpty() ? installLocation : installLocation + QLatin1Char('/') + relDir;

    const QString path = bestLogoCandidate(dir, baseName);
    if (!path.isEmpty())
        info.logo = QPixmap(path);
    return info;
}

} // namespace

DebloatScanner::DebloatScanner(QObject *parent)
    : QObject(parent)
{
}

void DebloatScanner::start()
{
    if (m_process)
        return;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString scriptPath = dir + QStringLiteral("/debloat_scan.ps1");

    // The same rule actionengine.cpp follows: the absolute path from winpaths.h, never
    // the bare name. This process is elevated and a bare "powershell.exe" is resolved
    // through the PATH it inherited from whoever started it. Resolved before the script
    // is written, so a machine without it fails with nothing left behind.
    const QString powershell = WinPaths::powershell();
    if (powershell.isEmpty()) {
        Q_EMIT finished({});
        return;
    }

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT finished({});
        return;
    }
    script.write("\xEF\xBB\xBF");

    // Provisioned packages are the debloat set: the apps Windows keeps staged in the image
    // and hands to every account, which is exactly what "preinstalled bloat" means. Plain
    // Get-AppxPackage is the wrong source — it answers "what is registered right now",
    // which sweeps in the shell's own system apps and anything the user installed
    // themselves. This query needs elevation; Arbitrium's manifest already guarantees it.
    //
    // Get-AppxPackage is still consulted, but only as a lookup: a provisioned entry carries
    // no install path (so no logo) and no Start-menu identity, both of which come from the
    // registered package when there is one.
    //
    // Get-StartApps is where the human-readable, already-localised names live: a manifest
    // DisplayName is usually an ms-resource indirection this process cannot resolve, but
    // Windows has resolved it for its own Start menu. Its AppID for a packaged app is
    // "<PackageFamilyName>!<AppId>", which is what joins it back to the package list.
    //
    // Those friendly names are localised, so this output carries Turkish letters, ®, and
    // whatever else the installed apps are called. PowerShell 5 writes a redirected stream
    // in the console's OEM codepage unless told otherwise, and those bytes are not valid
    // UTF-8 — QJsonDocument rejects the whole document and the list silently comes back
    // empty. Pinning both encodings is what keeps the names readable and the JSON parseable.
    script.write(
        "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8\n"
        "$OutputEncoding = [System.Text.Encoding]::UTF8\n"
        "$start = @{}\n"
        "try {\n"
        "  Get-StartApps -ErrorAction SilentlyContinue | ForEach-Object {\n"
        "    if ($_.AppID -match '^([^\\\\!]+_[a-z0-9]+)!') { $start[$Matches[1]] = $_.Name }\n"
        "  }\n"
        "} catch {}\n"
        "$installed = @{}\n"
        "foreach ($p in (@(Get-AppxPackage -AllUsers -ErrorAction SilentlyContinue) + "
        "@(Get-AppxPackage -ErrorAction SilentlyContinue))) {\n"
        "  if ($p -and $p.Name -and -not $installed.ContainsKey($p.Name)) { $installed[$p.Name] = $p }\n"
        "}\n"
        "$prov = @()\n"
        "try { $prov = @(Get-AppxProvisionedPackage -Online -ErrorAction Stop) } catch { $prov = @() }\n"
        "$rows = @()\n"
        "foreach ($e in $prov) {\n"
        "  $name = [string]$e.DisplayName\n"
        "  if (-not $name) { continue }\n"
        "  $p = $installed[$name]\n"
        "  $loc = ''; $sig = ''; $nonRem = $false; $sname = ''; $isInst = $false\n"
        "  if ($p) {\n"
        "    $isInst = $true\n"
        "    $loc = [string]$p.InstallLocation\n"
        "    $sig = [string]$p.SignatureKind\n"
        "    $nonRem = [bool]$p.NonRemovable\n"
        "    $sname = [string]$start[$p.PackageFamilyName]\n"
        "  }\n"
        "  $rows += [PSCustomObject]@{\n"
        "    Name = $name\n"
        "    PackageFullName = [string]$e.PackageName\n"
        "    InstallLocation = $loc\n"
        "    Version = [string]$e.Version\n"
        "    SignatureKind = $sig\n"
        "    NonRemovable = $nonRem\n"
        "    StartName = $sname\n"
        "    Installed = $isInst\n"
        "  }\n"
        "}\n"
        "ConvertTo-Json -InputObject @($rows) -Compress\n");
    script.close();

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setProgram(powershell);
    m_process->setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                             QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                             QStringLiteral("-File"), QDir::toNativeSeparators(scriptPath)});

    connect(m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        // A crashed process emits errorOccurred *and* finished, and the handler below
        // has already cleared and deleted it by the time this runs. Reading the output
        // off a null pointer is how a failed scan turned into a crash.
        QProcess *proc = m_process;
        if (!proc)
            return;
        m_process = nullptr;

        const QByteArray raw = proc->readAllStandardOutput();
        proc->deleteLater();

        QJsonParseError error{};
        QJsonDocument doc = QJsonDocument::fromJson(raw, &error);
        if (error.error != QJsonParseError::NoError) {
            // A machine whose console codepage defeats the UTF-8 pinning above would other-
            // wise report "nothing installed", which is the one answer that is never true.
            doc = QJsonDocument::fromJson(QString::fromLocal8Bit(raw).toUtf8(), &error);
        }

        QJsonArray rows;
        if (error.error == QJsonParseError::NoError) {
            if (doc.isArray())
                rows = doc.array();
            else if (doc.isObject())
                rows.append(doc.object());   // a single match does not come back as an array
        }

        QVector<InstalledApp> found;
        QSet<QString> seen;

        for (const QJsonValue &rv : std::as_const(rows)) {
            const QJsonObject ro = rv.toObject();

            InstalledApp app;
            app.packageName = ro.value(QStringLiteral("Name")).toString();
            if (app.packageName.isEmpty() || seen.contains(app.packageName))
                continue;
            seen.insert(app.packageName);

            app.packageFullName = ro.value(QStringLiteral("PackageFullName")).toString();
            app.installLocation = ro.value(QStringLiteral("InstallLocation")).toString();
            app.version = ro.value(QStringLiteral("Version")).toString();

            const QString startName = ro.value(QStringLiteral("StartName")).toString();
            const QString signature = ro.value(QStringLiteral("SignatureKind")).toString();
            const bool nonRemovable = ro.value(QStringLiteral("NonRemovable")).toBool();

            const ManifestInfo info = readManifest(app.installLocation);
            app.logo = info.logo;
            app.publisher = info.publisher;

            // Best name first: what Windows shows in Start, then what the manifest states
            // outright, and only then the raw package name.
            app.displayName = !startName.isEmpty()          ? startName
                              : !info.displayName.isEmpty() ? info.displayName
                                                            : app.packageName;

            app.installed = ro.value(QStringLiteral("Installed")).toBool();
            app.userFacing = !startName.isEmpty();
            app.systemComponent =
                nonRemovable
                || signature.compare(QLatin1String("System"), Qt::CaseInsensitive) == 0;
            app.removable = !app.systemComponent && !isCritical(app.packageName);

            found.append(app);
        }

        // Apps first, then background components, then what is only staged; alphabetical
        // by the name actually on screen within each.
        std::sort(found.begin(), found.end(), [](const InstalledApp &a, const InstalledApp &b) {
            if (a.section() != b.section())
                return a.section() < b.section();
            return a.displayName.localeAwareCompare(b.displayName) < 0;
        });

        Q_EMIT finished(found);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        QProcess *proc = m_process;
        if (!proc)
            return;
        m_process = nullptr;
        proc->deleteLater();
        Q_EMIT finished({});
    });

    m_process->start();
}

QString DebloatActions::removalScript(const QStringList &packageNames)
{
    QStringList lines;
    lines << QStringLiteral("$removed = New-Object System.Collections.Generic.List[string]");
    lines << QStringLiteral("$failed  = New-Object System.Collections.Generic.List[string]");

    for (const QString &name : packageNames) {
        const QString esc = QString(name).replace(QLatin1Char('\''), QStringLiteral("''"));

        // Ask afterwards rather than assume. Both removals are -ErrorAction
        // SilentlyContinue, which is deliberate — a package present for one account and
        // not another is normal — but it also means powershell.exe exits 0 whatever
        // happened. The old script simply added every requested name to $removed, so a
        // package Windows refuses to remove was reported as removed, and the list was then
        // re-scanned and showed it still sitting there.
        //
        // -eq against $_.Name, not -Name '<pattern>': the -Name parameter takes a
        // wildcard, so a name containing [ or * matched something else or nothing.
        lines << QStringLiteral("$n = '%1'").arg(esc);
        lines << QStringLiteral("$gone = $true");
        lines << QStringLiteral(
            "if (@(Get-AppxPackage -AllUsers -ErrorAction SilentlyContinue | "
            "Where-Object { $_.Name -eq $n }).Count) {"
            " Get-AppxPackage -AllUsers -ErrorAction SilentlyContinue | "
            "Where-Object { $_.Name -eq $n } | "
            "Remove-AppxPackage -AllUsers -ErrorAction SilentlyContinue;"
            " if (@(Get-AppxPackage -AllUsers -ErrorAction SilentlyContinue | "
            "Where-Object { $_.Name -eq $n }).Count) { $gone = $false } }");
        lines << QStringLiteral(
            "if (@(Get-AppxProvisionedPackage -Online -ErrorAction SilentlyContinue | "
            "Where-Object { $_.DisplayName -eq $n }).Count) {"
            " Get-AppxProvisionedPackage -Online -ErrorAction SilentlyContinue | "
            "Where-Object { $_.DisplayName -eq $n } | "
            "Remove-AppxProvisionedPackage -Online -ErrorAction SilentlyContinue | Out-Null;"
            " if (@(Get-AppxProvisionedPackage -Online -ErrorAction SilentlyContinue | "
            "Where-Object { $_.DisplayName -eq $n }).Count) { $gone = $false } }");
        lines << QStringLiteral("if ($gone) { $removed.Add($n) } else { $failed.Add($n) }");
    }

    // Machine-readable, not a sentence: the script has no idea what language the interface
    // is in, and a translated format string would have to survive PowerShell quoting on the
    // way through. DebloatPage turns this into words with Locale::tr instead. The fourth
    // field is the one the old token had no room for — what did not come off.
    lines << QStringLiteral(
        "Write-Output (\"ARB-REMOVED|{0}|{1}|{2}\" -f $removed.Count, "
        "($removed -join ', '), ($failed -join ', '))");
    return lines.join(QLatin1Char('\n'));
}
