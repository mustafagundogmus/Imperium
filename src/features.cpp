#include "features.h"
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
#include <QStandardPaths>
#include <QTextStream>

namespace Features {

namespace {

const char *const Preamble = R"PS(
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
$ProgressPreference = 'SilentlyContinue'

function Write-WinUtilLog {
    param(
        [string]$Component = "General",
        [string]$Message = "",
        [string]$Level = "INFO"
    )
    Write-Output "ARB-LOG|$Level|$Component|$Message"
}

function Invoke-WinUtilFeatureInstall ($CheckBox) {
    Write-WinUtilLog -Component "Feature" -Message "Applying feature action: $CheckBox"

    if ($sync.configs.feature.$CheckBox.feature) {
        foreach ($feature in $sync.configs.feature.$CheckBox.feature) {
            Write-Host "Installing $feature"
            Write-WinUtilLog -Component "Feature" -Message "Enabling Windows optional feature: $feature"
            Enable-WindowsOptionalFeature -Online -FeatureName $feature -All -NoRestart -ErrorAction Stop
            Write-WinUtilLog -Component "Feature" -Message "Enabled Windows optional feature: $feature"
        }
    }

    if ($sync.configs.feature.$CheckBox.InvokeScript) {
        foreach ($script in $sync.configs.feature.$CheckBox.InvokeScript) {
            Write-Host "Running Script for $CheckBox"
            Write-WinUtilLog -Component "Feature" -Message "Running feature script for: $CheckBox"
            Invoke-Command -ScriptBlock ([scriptblock]::Create($script)) -ErrorAction Stop
            Write-WinUtilLog -Component "Feature" -Message "Completed feature script for: $CheckBox"
        }
    }
    Write-WinUtilLog -Component "Feature" -Message "Feature action completed: $CheckBox"
}
)PS";

QString psQuote(const QString &s)
{
    return QLatin1Char('\'') + QString(s).replace(QLatin1Char('\''), QStringLiteral("''"))
           + QLatin1Char('\'');
}

QString psArray(const QStringList &items)
{
    QStringList quoted;
    for (const QString &s : items)
        quoted << psQuote(s);
    return QStringLiteral("@(") + quoted.join(QStringLiteral(", ")) + QLatin1Char(')');
}

/// WinUtil's function reads its rows out of $sync.configs.feature; the same table, built
/// from the rows the run covers, is what lets the function stay word for word.
QString syncTable(const QVector<Entry> &entries)
{
    QString s = QStringLiteral("$sync = @{ configs = @{ feature = @{} } }\n");
    for (const Entry &e : entries) {
        s += QStringLiteral("$sync.configs.feature[%1] = @{ feature = %2; InvokeScript = %3 }\n")
                 .arg(psQuote(e.key), psArray(e.features), psArray(e.scripts));
    }
    return s;
}

QString installBody(const QVector<Entry> &entries)
{
    QStringList keys;
    for (const Entry &e : entries)
        keys << e.key;

    QString body = syncTable(entries);
    body += QStringLiteral("$Features = %1\n").arg(psArray(keys));
    body += QStringLiteral(
        "$x = 0\n"
        "$Features | ForEach-Object {\n"
        "    $key = $_\n"
        "    Write-Output \"ARB-FEAT|start|$($x + 1)|$($Features.Count)|$key\"\n"
        "    try {\n"
        "        Invoke-WinUtilFeatureInstall $key\n"
        "        Write-Output \"ARB-FEAT|done|$($x + 1)|$($Features.Count)|$key|ok\"\n"
        "    } catch {\n"
        "        Write-WinUtilLog -Level \"ERROR\" -Component \"Feature\" -Message \"Feature action failed: $key ($($_.Exception.Message))\"\n"
        "        Write-Output \"ARB-FEAT|done|$($x + 1)|$($Features.Count)|$key|failed\"\n"
        "    }\n"
        "    $x++\n"
        "}\n"
        "Write-Host \"===================================\"\n"
        "Write-Host \"---   Features are Installed    ---\"\n"
        "Write-Host \"---  A Reboot may be required   ---\"\n"
        "Write-Host \"===================================\"\n");
    return body;
}

QString disableBody(const QStringList &features)
{
    QString body = QStringLiteral("$Features = %1\n").arg(psArray(features));
    body += QStringLiteral(
        "$x = 0\n"
        "$Features | ForEach-Object {\n"
        "    $feature = $_\n"
        "    Write-Output \"ARB-FEAT|start|$($x + 1)|$($Features.Count)|$feature\"\n"
        "    try {\n"
        "        Write-WinUtilLog -Component \"Feature\" -Message \"Disabling Windows optional feature: $feature\"\n"
        "        Disable-WindowsOptionalFeature -Online -FeatureName $feature -NoRestart -ErrorAction Stop | Out-Null\n"
        "        Write-WinUtilLog -Component \"Feature\" -Message \"Disabled Windows optional feature: $feature\"\n"
        "        Write-Output \"ARB-FEAT|done|$($x + 1)|$($Features.Count)|$feature|ok\"\n"
        "    } catch {\n"
        "        Write-WinUtilLog -Level \"ERROR\" -Component \"Feature\" -Message \"Disable failed: $feature ($($_.Exception.Message))\"\n"
        "        Write-Output \"ARB-FEAT|done|$($x + 1)|$($Features.Count)|$feature|failed\"\n"
        "    }\n"
        "    $x++\n"
        "}\n");
    return body;
}

/// The three facts the rows are drawn from, as one compact JSON object between markers.
/// Get-WindowsOptionalFeature is the slow part — several seconds — and the only one that
/// needs DISM; the other two are a bcdedit line and a registry value.
QString scanBody()
{
    return QStringLiteral(
        "$feats = @()\n"
        "try {\n"
        "    $feats = @(Get-WindowsOptionalFeature -Online -ErrorAction Stop | ForEach-Object {\n"
        "        @{ Name = [string]$_.FeatureName; State = [string]$_.State }\n"
        "    })\n"
        "} catch { Write-WinUtilLog -Level \"ERROR\" -Component \"Scan\" -Message \"Get-WindowsOptionalFeature failed: $($_.Exception.Message)\" }\n"
        "$policy = ''\n"
        "try {\n"
        "    $lines = @(& \"$env:SystemRoot\\System32\\bcdedit.exe\" /enum '{current}' 2>&1)\n"
        "    foreach ($l in $lines) { if ([string]$l -match '^bootmenupolicy\\s+(\\S+)') { $policy = $Matches[1] } }\n"
        "} catch {}\n"
        "$reg = 0\n"
        "try { $reg = [int](Get-ItemProperty -Path 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Configuration Manager' -Name 'EnablePeriodicBackup' -ErrorAction Stop).EnablePeriodicBackup } catch {}\n"
        "$task = $false\n"
        "try { $task = [bool](Get-ScheduledTask -TaskName 'AutoRegBackup' -ErrorAction SilentlyContinue) } catch {}\n"
        "$result = [PSCustomObject]@{ Features = $feats; BootMenuPolicy = $policy; RegBackup = ($reg -eq 1 -and $task) }\n"
        "Write-Output \"ARB-JSON-BEGIN\"\n"
        "Write-Output (ConvertTo-Json -InputObject $result -Compress -Depth 4)\n"
        "Write-Output \"ARB-JSON-END\"\n");
}

} // namespace

// ------------------------------------------------------------- Catalogue ---

QString slugFor(const QString &key)
{
    QString s = key;
    for (const char *prefix : {"WPFFeatures", "WPFFeature"}) {
        if (s.startsWith(QLatin1String(prefix))) {
            s = s.mid(int(qstrlen(prefix)));
            break;
        }
    }
    return s.toLower();
}

const Catalogue &Catalogue::instance()
{
    static const Catalogue c;
    return c;
}

Catalogue::Catalogue()
{
    QFile file(QStringLiteral(":/data/features.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("features.json: cannot open resource");
        return;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning("features.json: %s", qUtf8Printable(error.errorString()));
        return;
    }

    // QJsonObject iterates in key order, not file order; the file's own order is the one
    // WinUtil draws, so it is read back off the raw text.
    file.seek(0);
    const QString raw = QString::fromUtf8(file.readAll());
    QStringList order;
    {
        int from = 0;
        while (true) {
            const int q = raw.indexOf(QStringLiteral("\n  \""), from);
            if (q < 0)
                break;
            const int end = raw.indexOf(QLatin1Char('"'), q + 4);
            if (end < 0)
                break;
            order << raw.mid(q + 4, end - (q + 4));
            from = end;
        }
    }

    const QJsonObject root = doc.object();
    for (const QString &key : std::as_const(order)) {
        const QJsonValue v = root.value(key);
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("category")).toString() != QLatin1String("Features"))
            continue;
        if (o.value(QStringLiteral("Type")).toString().compare(QLatin1String("Button"),
                                                                Qt::CaseInsensitive) == 0)
            continue;   // the Install Features button is the page's own

        Entry e;
        e.key = key;
        e.slug = slugFor(key);
        e.name = o.value(QStringLiteral("Content")).toString().trimmed();
        e.description = o.value(QStringLiteral("Description")).toString().trimmed();
        e.link = o.value(QStringLiteral("link")).toString().trimmed();
        for (const QJsonValue &f : o.value(QStringLiteral("feature")).toArray())
            if (f.isString() && !f.toString().trimmed().isEmpty())
                e.features << f.toString().trimmed();
        for (const QJsonValue &s : o.value(QStringLiteral("InvokeScript")).toArray())
            if (s.isString() && !s.toString().trimmed().isEmpty())
                e.scripts << s.toString();
        if (e.features.isEmpty() && e.scripts.isEmpty())
            continue;
        m_index.insert(e.key, int(m_entries.size()));
        m_entries.append(e);
    }
}

const Entry *Catalogue::entry(const QString &key) const
{
    const int i = m_index.value(key, -1);
    return i >= 0 ? &m_entries.at(i) : nullptr;
}

// --------------------------------------------------------------- Machine ---

State Machine::stateOf(const Entry &e) const
{
    if (!valid)
        return State::Unknown;

    if (e.isDism()) {
        int on = 0, off = 0, missing = 0;
        for (const QString &f : e.features) {
            const QString s = featureStates.value(f);
            if (s.isEmpty())
                ++missing;
            else if (s.startsWith(QLatin1String("Enable"), Qt::CaseInsensitive))
                ++on;   // Enabled, EnablePending
            else
                ++off;  // Disabled, DisabledWithPayloadRemoved, DisablePending
        }
        if (on == 0 && off == 0)
            return State::Unavailable;
        if (on > 0 && off == 0 && missing == 0)
            return State::Enabled;
        if (on == 0)
            return State::Disabled;
        return State::Partial;
    }

    if (e.slug == QLatin1String("regbackup"))
        return regBackup ? State::Enabled : State::Disabled;
    if (e.slug == QLatin1String("enablelegacyrecovery")) {
        if (bootMenuPolicy.isEmpty())
            return State::Unknown;
        return bootMenuPolicy.compare(QLatin1String("Legacy"), Qt::CaseInsensitive) == 0
                   ? State::Enabled
                   : State::Disabled;
    }
    if (e.slug == QLatin1String("disablelegacyrecovery")) {
        if (bootMenuPolicy.isEmpty())
            return State::Unknown;
        return bootMenuPolicy.compare(QLatin1String("Standard"), Qt::CaseInsensitive) == 0
                   ? State::Enabled
                   : State::Disabled;
    }
    return State::Unknown;
}

// ------------------------------------------------------------- summaries ---

QString installSummary(const QVector<Entry> &entries)
{
    QStringList lines;
    for (const Entry &e : entries) {
        lines << QStringLiteral("# ") + e.key;
        for (const QString &f : e.features)
            lines << QStringLiteral("Enable-WindowsOptionalFeature -Online -FeatureName %1 -All -NoRestart").arg(f);
        for (const QString &s : e.scripts)
            lines << s.trimmed();
    }
    return lines.join(QLatin1Char('\n'));
}

QString disableSummary(const QStringList &features)
{
    QStringList lines;
    for (const QString &f : features)
        lines << QStringLiteral("Disable-WindowsOptionalFeature -Online -FeatureName %1 -NoRestart").arg(f);
    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------- Runner ---

Runner::Runner(QObject *parent)
    : QObject(parent)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_scriptPath = dir + QStringLiteral("/features.ps1");
    m_logPath = dir + QStringLiteral("/features.log");
}

void Runner::scan()
{
    start(Job::Scan, scanBody(), 0);
}

void Runner::install(const QVector<Entry> &entries)
{
    start(Job::Install, installBody(entries), int(entries.size()));
}

void Runner::disable(const QStringList &features)
{
    start(Job::Disable, disableBody(features), int(features.size()));
}

void Runner::start(Job job, const QString &body, int total)
{
    if (m_process)
        return;

    const QString powershell = WinPaths::powershell();
    QFile script(m_scriptPath);
    if (powershell.isEmpty() || !script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_job = job;
        Q_EMIT started(job, total);
        m_job = Job::None;
        if (job == Job::Scan)
            Q_EMIT scanned(Machine{});
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
    m_collecting = false;
    m_carry.clear();
    m_json.clear();
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
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter))
        text = Console::decode(raw);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
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

    if (text == QLatin1String("ARB-JSON-BEGIN")) {
        m_collecting = true;
        return;
    }
    if (text == QLatin1String("ARB-JSON-END")) {
        m_collecting = false;
        return;
    }
    if (m_collecting) {
        m_json += text;
        return;
    }

    if (text.startsWith(QLatin1String("ARB-FEAT|"))) {
        const QStringList f = text.split(QLatin1Char('|'));
        const QString kind = f.value(1);
        const int index = f.value(2).toInt();
        const QString key = f.value(4);
        if (kind == QLatin1String("start")) {
            Q_EMIT progress(index - 1, m_total, key, false, true);
        } else if (kind == QLatin1String("done")) {
            m_done = index;
            const bool ok = f.value(5) == QLatin1String("ok");
            if (!ok)
                ++m_failures;
            Q_EMIT progress(m_done, m_total, key, true, ok);
        }
        return;
    }

    if (text.startsWith(QLatin1String("ARB-LOG|"))) {
        const QStringList f = text.split(QLatin1Char('|'));
        Q_EMIT line(QStringLiteral("[%1] %2").arg(f.value(2), f.mid(3).join(QLatin1Char('|'))));
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
        // The scan's JSON is data, not a transcript worth keeping a copy of.
        if (m_job != Job::Scan && !m_transcript.isEmpty())
            out << m_transcript;
        out << "---\n";
    }

    const Job job = m_job;
    m_job = Job::None;
    m_process->deleteLater();
    m_process = nullptr;

    bool ok = exitCode == 0;

    if (job == Job::Scan) {
        Machine machine;
        QJsonParseError error{};
        QJsonDocument doc = QJsonDocument::fromJson(m_json.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject o = doc.object();
            QJsonValue feats = o.value(QStringLiteral("Features"));
            QJsonArray array;
            if (feats.isArray())
                array = feats.toArray();
            else if (feats.isObject())
                array.append(feats.toObject());   // a single feature does not come back as an array
            for (const QJsonValue &fv : std::as_const(array)) {
                const QJsonObject fo = fv.toObject();
                const QString name = fo.value(QStringLiteral("Name")).toString();
                if (!name.isEmpty())
                    machine.featureStates.insert(name, fo.value(QStringLiteral("State")).toString());
            }
            machine.bootMenuPolicy = o.value(QStringLiteral("BootMenuPolicy")).toString();
            machine.regBackup = o.value(QStringLiteral("RegBackup")).toBool();
            machine.valid = !machine.featureStates.isEmpty();
        }
        ok = ok && machine.valid;
        Q_EMIT scanned(machine);
    }

    Q_EMIT finished(job, ok, m_failures);
}

} // namespace Features
