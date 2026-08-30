// godmodepage.h — the "God Mode" screen: a searchable way into Windows' own settings.
//
// Every other page in this app changes the machine. This one does not: each row hands a
// target to Windows — a Settings page, a control-panel applet, a management console — and
// Windows draws its own dialog. Nothing is written, nothing is queued, and there is no
// confirmation step, because there is nothing to confirm; the row says exactly what it
// opens and opening it is the whole operation.
//
// The list itself is data (resources/data/settings-links.json, compiled into the binary —
// see settingslinks.h for why it is not read off disk). A SearchField over the top filters
// the rows by what they say in the interface language and by the target they name, so
// somebody who knows "ncpa.cpl" finds the same row as somebody looking for "Ağ bağlantıları".
//
// The launch itself splits three ways, decided from the target string in godmodepage.cpp:
// a URI goes through QDesktopServices::openUrl; everything else goes through
// QProcess::startDetached with an *absolute* program path resolved from GetSystemDirectory.
// Never a bare name: this process is elevated and is routinely run out of Downloads, so a
// bare "mmc.exe" would let a file sitting next to the executable win the search order. It
// is the same class of bug this project already fixed for tbs.dll and netapi32.dll.

#pragma once

#include <QVector>
#include <QWidget>

#include "../settingslinks.h"

class PillButton;
class QLabel;
class SearchField;
class SectionHeader;
class SettingRow;

class GodModePage : public QWidget
{
    Q_OBJECT

public:
    explicit GodModePage(QWidget *parent = nullptr);

    /// Links the page drew — what the header subtitle counts.
    int rowCount() const { return int(m_links.size()); }

Q_SIGNALS:
    void notice(const QString &text);

private:
    /// One row: the link it came from, the widgets drawing it, and the resolved command.
    /// The command is worked out once at build time rather than on click, because whether
    /// it resolved at all is what the row has to say for itself — gpedit.msc is simply not
    /// on a Home edition, and a row that only admits that after being clicked is a worse
    /// row than one that says so up front.
    struct Row
    {
        SettingsLinks::Link link;
        SettingRow *row = nullptr;
        PillButton *button = nullptr;
        QString program;        ///< absolute path, empty for a URI or an unresolved target
        QStringList arguments;
        bool available = true;
    };

    struct Group
    {
        QString id;
        SectionHeader *header = nullptr;
        QWidget *block = nullptr;
        QVector<int> rows;   ///< indices into m_links
    };

    void build();
    void launch(int index);
    void applyFilter();
    void retranslate();

    QString label(const SettingsLinks::Link &link) const;

    SearchField *m_search = nullptr;
    QLabel *m_intro = nullptr;
    QLabel *m_empty = nullptr;
    QVector<Row> m_links;
    QVector<Group> m_groups;
};
