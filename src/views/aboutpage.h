// aboutpage.h — the Hakkında screen.
//
// Its own page rather than a section at the bottom of Ayarlar: who built this, where the
// code lives, where to report something broken, and where to leave a tip if the app was
// worth one. None of it reads or writes a setting, which is the one thing every row on
// the settings page has in common and this has none of — it earned a page of its own.

#pragma once

#include <QWidget>

class SettingRow;

class AboutPage : public QWidget
{
    Q_OBJECT

public:
    explicit AboutPage(QWidget *parent = nullptr);

    int rowCount() const { return m_rowCount; }

private:
    void retranslate();

    SettingRow *m_developerRow = nullptr;
    SettingRow *m_sourceRow = nullptr;
    SettingRow *m_issuesRow = nullptr;
    SettingRow *m_donateRow = nullptr;
    int m_rowCount = 0;
};
