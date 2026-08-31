#include "actionengine.h"
#include "console.h"
#include "winpaths.h"
#include "i18n.h"

#include "action.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

ActionEngine::ActionEngine(QObject *parent)
    : QObject(parent)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_logPath = dir + QStringLiteral("/actions.log");
    m_scriptPath = dir + QStringLiteral("/action.ps1");
}

void ActionEngine::run(const Action &action)
{
    if (m_process)
        return;

    // Resolved before the script is written, so a machine that cannot run this fails with
    // one message and no leftover file. A bare name would be resolved by QProcess against
    // PATH, in an elevated process, for a program about to be handed a script — the same
    // hole winpaths.h closes for the two probes. There is deliberately no fallback to the
    // bare name: nothing else should be allowed to answer to "powershell" here.
    const QString powershell = WinPaths::powershell();
    if (powershell.isEmpty()) {
        Q_EMIT finished(action.id, false, Locale::tr(QStringLiteral("err.noPowerShell")));
        return;
    }

    // A BOM, because the scripts carry Turkish text and PowerShell 5 reads a file without
    // one as the system codepage.
    QFile script(m_scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT finished(action.id, false, Locale::tr(QStringLiteral("err.scriptWrite")));
        return;
    }
    script.write("\xEF\xBB\xBF");
    script.write(action.script().toUtf8());
    script.write("\n");
    script.close();

    m_runningId = action.id;
    m_runningName = action.name;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setProgram(powershell);
    m_process->setArguments({QStringLiteral("-NoProfile"),
                             QStringLiteral("-NonInteractive"),
                             QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                             QStringLiteral("-File"), QDir::toNativeSeparators(m_scriptPath)});

    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus) { onFinished(exitCode); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process)
            onFinished(-1);
    });

    // Announced before the launch, not after it. QProcess::start() reports a failure to
    // launch synchronously — errorOccurred fires from inside the call — so emitting
    // started() afterwards delivered finished() first: the page re-enabled the row, wrote
    // the result, and was then told the action had just begun, leaving it dimmed and
    // reading "çalışıyor" forever. m_process is already set here, so running() is true
    // for anything the signal reaches.
    Q_EMIT started(action.id);
    m_process->start();
}

void ActionEngine::onFinished(int exitCode)
{
    if (!m_process)
        return;

    // DISM, cleanmgr, icacls and powershell's own error text all reach here through one
    // pipe, and the action page prints it verbatim. fromLocal8Bit read it with the ANSI
    // code page while the console wrote it with the OEM one — see console.h.
    const QString output = Console::decode(m_process->readAll()).trimmed();
    const bool ok = exitCode == 0;

    QFile log(m_logPath);
    if (log.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&log);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << "  " << m_runningName
            << "  (" << m_runningId << ")  exit=" << exitCode << '\n';
        if (!output.isEmpty())
            out << output << '\n';
        out << "---\n";
    }

    const QString id = m_runningId;
    m_runningId.clear();
    m_runningName.clear();

    m_process->deleteLater();
    m_process = nullptr;

    Q_EMIT finished(id, ok, output);
}
