// aboutpage.h — the Hakkında screen.
//
// Its own page rather than a section at the bottom of Ayarlar: what this application is,
// what it carries, who built it, where the code lives, where to report something broken,
// and where to leave a tip if it was worth one. None of it reads or writes a setting,
// which is the one thing every row on the settings page has in common and this has none
// of — it earned a page of its own.
//
// Two InfoSection cards over the four link rows, and nothing else. The cards are the very
// ones the Genel Bakış grid is built from rather than a second card that would look almost
// like it, and every figure on them is read back out of the running application (the
// catalogue's own count, the languages the table actually carries, the faces resources.qrc
// actually holds), so the page cannot claim a number this build does not have.
//
// It says nothing about itself in prose. The content header above it is already carrying a
// title and a subtitle for this page, and a third line of "what Arbitrium is" underneath
// those, in the same 15px semibold the header's title is set in, is the page arguing with
// its own heading.

#pragma once

#include <QWidget>

class InfoSection;
class SettingRow;

class AboutPage : public QWidget
{
    Q_OBJECT

public:
    explicit AboutPage(QWidget *parent = nullptr);

    int rowCount() const { return m_rowCount; }

private:
    void retranslate();

    InfoSection *m_appCard = nullptr;
    InfoSection *m_lookCard = nullptr;

    SettingRow *m_developerRow = nullptr;
    SettingRow *m_sourceRow = nullptr;
    SettingRow *m_issuesRow = nullptr;
    SettingRow *m_donateRow = nullptr;
    int m_rowCount = 0;
};
