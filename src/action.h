// action.h — the things that are not tweaks.
//
// A tweak is a position: you set it, the app can read it back, and you can put it where
// it was. An action is none of those. Removing Edge, emptying the temp folders, resetting
// the DNS on every adapter — these run once, and the app cannot look at the machine
// afterwards and tell you what the previous state had been.
//
// So they are kept apart from the catalogue, in their own file with their own page. Each
// one carries the exact PowerShell it will run, in plain text, because the confirmation
// dialog shows it: an action you cannot undo is one you should be able to read first.

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct Action
{
    QString id;
    QString name;
    QString desc;
    bool reversible = false;   ///< there is a documented way back, described in `note`
    QString note;              ///< the caveat the confirmation spells out
    QStringList run;           ///< PowerShell, one line per entry

    /// The whole script, preamble included.
    ///
    /// PowerShell's default is to print a cmdlet error, carry on to the next line, and
    /// exit 0 — and every script here ends by printing its own ARB result line, which is
    /// a cmdlet and always succeeds. So an action whose real work failed reported success,
    /// and the line the user was shown was the one written for a clean run. `Stop` makes a
    /// cmdlet error terminating, powershell.exe exits non-zero for that, and that is what
    /// ActionEngine already reads. The lines that are *meant* to fail quietly say so
    /// themselves with -ErrorAction SilentlyContinue, and a parameter beats the preference.
    ///
    /// Prepended here rather than in ActionEngine::run() on purpose: the confirmation
    /// dialog shows this string, and the promise at the top of this file is that what you
    /// read is what runs.
    QString script() const
    {
        return QStringLiteral("$ErrorActionPreference = 'Stop'\n") + run.join(QLatin1Char('\n'));
    }

    /// The text in the interface language, falling back to the Turkish in actions.json
    /// when this action has not been translated yet — same contract as Tweak's.
    QString displayName() const;
    QString displayDesc() const;
    QString displayNote() const;
};

struct ActionSection
{
    QString title;
    QVector<Action> actions;

    QString displayTitle() const;
};

/// Parses :/data/actions.json once.
class ActionCatalog
{
public:
    static const ActionCatalog &instance();

    const QVector<ActionSection> &sections() const { return m_sections; }
    int total() const;

private:
    ActionCatalog();

    QVector<ActionSection> m_sections;
};
