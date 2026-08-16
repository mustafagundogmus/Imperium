// journalpage.h — what this app has changed on this machine.
//
// Every write goes through TweakEngine::journal() first, which records the value that was
// there, its type, and whether the key itself existed. That file has been accumulating
// since the first version; this is the screen that finally reads it back.
//
// The point is not history for its own sake. A tweak's own switch can put back what the
// catalogue thinks the default is; only the journal knows what *this* machine had. So each
// row can be reverted on its own, straight from the recorded value, even for a tweak whose
// definition has changed since — or one that has been removed from the catalogue entirely.

#pragma once

#include <QHash>
#include <QVector>
#include <QWidget>

#include "../tweakengine.h"

class AppState;
class PillButton;
class SettingRow;
class QVBoxLayout;

class JournalPage : public QWidget
{
    Q_OBJECT

public:
    JournalPage(TweakEngine *engine, AppState *state, QWidget *parent = nullptr);

    /// Re-reads the file. Called when the page is shown, since an apply may have run.
    void reload();

    int rowCount() const { return m_rowCount; }

Q_SIGNALS:
    void notice(const QString &text);

private:
    void revert(int index);

    TweakEngine *m_engine = nullptr;
    AppState *m_state = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QWidget *m_body = nullptr;

    QVector<TweakEngine::JournalEntry> m_entries;
    QHash<int, SettingRow *> m_rows;
    QHash<int, PillButton *> m_buttons;
    int m_rowCount = 0;
};
