// actionpage.h — the Eylemler screen.
//
// Built from the same parts as the settings page (section headers, rows, pill buttons) so
// it reads as part of the app, but it is deliberately not the tweak list: nothing here has
// a switch, because nothing here has two states the app can read back.
//
// Every run goes through a confirmation that names what will happen, says whether there
// is a way back, and shows the script itself. The app is allowed to change the machine
// only in ways the user could have checked first.

#pragma once

#include <QHash>
#include <QVector>
#include <QWidget>

class ActionEngine;
class PillButton;
class SectionHeader;
class SettingRow;
struct Action;

class ActionPage : public QWidget
{
    Q_OBJECT

public:
    explicit ActionPage(QWidget *parent = nullptr);

    int rowCount() const { return m_rowCount; }

Q_SIGNALS:
    void notice(const QString &text);

private:
    QWidget *buildSection(const struct ActionSection &section);
    void confirmAndRun(const Action &action);
    void retranslate();

    ActionEngine *m_engine = nullptr;
    QHash<QString, PillButton *> m_buttons;
    QHash<QString, SettingRow *> m_rows;
    QVector<SectionHeader *> m_sectionHeaders;
    int m_rowCount = 0;
};
