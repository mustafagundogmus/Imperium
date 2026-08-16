#include "actionengine.h"
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
    m_process->setProgram(QStringLiteral("powershell.exe"));
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

    m_process->start();
    Q_EMIT started(action.id);
}

void ActionEngine::onFinished(int exitCode)
{
    if (!m_process)
        return;

    const QString output = QString::fromLocal8Bit(m_process->readAll()).trimmed();
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
