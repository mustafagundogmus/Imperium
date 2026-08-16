// actionengine.h — runs one action, and only one at a time.
//
// The script goes to PowerShell through a file rather than through -Command: the actions
// are multi-line, they contain quotes and Turkish text, and quoting all of that through a
// command line correctly is a losing game. -NoProfile keeps a user's own profile from
// changing what runs, -NonInteractive makes a prompt an error instead of a hang.
//
// Everything the run prints is captured and appended to actions.log next to the registry
// journal, so an action that went wrong can be read afterwards.

#pragma once

#include <QObject>
#include <QString>

class QProcess;
struct Action;

class ActionEngine : public QObject
{
    Q_OBJECT

public:
    explicit ActionEngine(QObject *parent = nullptr);

    bool running() const { return m_process != nullptr; }
    QString runningId() const { return m_runningId; }

    /// Starts \a action. Does nothing when one is already running.
    void run(const Action &action);

    QString logPath() const { return m_logPath; }

Q_SIGNALS:
    void started(const QString &id);
    void finished(const QString &id, bool ok, const QString &output);

private:
    void onFinished(int exitCode);

    QProcess *m_process = nullptr;
    QString m_runningId;
    QString m_runningName;
    QString m_scriptPath;
    QString m_logPath;
};
