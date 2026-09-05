// sidebar.h — §2 of the handoff.
//
//   212px wide, right border 1px #1D1D22, padding 10px 8px 0
//   search field inset a further 2px, 10px of space below it
//   category rows 28px tall with a 1px gap
//   bottom block: top border 1px #1D1D22, the pinned Ayarlar row, 12px below it
//
// The handoff also put the restore point in that bottom block. It is a setting, not a
// place to navigate to, so it lives on the settings page now.
//
// The catalogue's own category order is a flat list of twelve — Genel Bakış and eleven
// others with nothing to say which of them are related. Grouped into six clusters with
// a small header over each, the same list reads instead of just being scanned. Genel
// Bakış stays outside every cluster: it is the one row that is not a settings area, it
// is the dashboard, and grouping it with anything else would say it belongs to a
// category the way the others do.
//
// Two of the rows inside those clusters are not catalogue categories at all: Uygulamalar
// under Sistem and God Mode under Araçlar. Where a row belongs is a question about what
// the user came to do, not about which file its data lives in, so both sit where they
// read rather than in the pinned strip at the bottom. buildList() names both.
//
// Geometry is assigned by hand rather than by a QLayout: the design specifies the
// margins in CSS terms (a margin that collapses into the parent's padding) and hand
// placement keeps those numbers literal.

#pragma once

#include <QPair>
#include <QVector>
#include <QWidget>

class AppState;
class CategoryRow;
class SearchField;
class SectionHeader;
class SmoothScrollArea;

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(AppState *state, QWidget *parent = nullptr);

    SearchField *search() const { return m_search; }

    /// Category id used by the pinned Ayarlar row at the bottom of the list.
    static QString settingsId() { return QStringLiteral("settings"); }

    /// …and by the pinned Eylemler row beside it. Actions are not tweaks, so they are
    /// not a catalogue category either.
    static QString actionsId() { return QStringLiteral("actions"); }

    /// …and the app-removal row, filed under the Sistem group in the list: a live machine
    /// scan, not a real position in the catalogue, so it still needs its own id here.
    static QString debloatId() { return QStringLiteral("debloat"); }

    /// …and the disk cleaner, filed under Dosyalar beside Temizlik: what that category
    /// switches, this one empties. A live measurement of the disk, not a catalogue row.
    static QString cleanerId() { return QStringLiteral("cleaner"); }

    /// …and the settings launcher, filed under the new Araçlar group at the end of the
    /// list. Also not a catalogue category: it changes nothing, it opens Windows' own
    /// pages, so there is no tweak for the catalogue to carry.
    static QString godModeId() { return QStringLiteral("godmode"); }

    /// …and the Office download & install page.
    static QString officeId() { return QStringLiteral("office"); }

    /// …and the write history, which is not a category either.
    static QString journalId() { return QStringLiteral("journal"); }

    /// …and the TrustedInstaller launcher, a tool rather than a catalogue category — it
    /// starts a program under the account that owns the files an administrator cannot.
    static QString tiLauncherId() { return QStringLiteral("tilauncher"); }

    /// …and the app installer, filed under Sistem right after the app-removal row: the
    /// one lists what Windows put here, the other what WinGet and Chocolatey can add.
    /// A catalogue of its own (applications.json), not a position in this one.
    static QString appsId() { return QStringLiteral("apps"); }

    /// …and the Windows optional features, right after it: WinUtil's Features section,
    /// read against DISM. Its own file (features.json), not a catalogue category.
    static QString featuresId() { return QStringLiteral("features"); }

    /// True for any of the ids above: pages of their own, none of which the catalogue
    /// knows about. The comparisons were written out at three separate call sites in
    /// MainWindow, which is three places to forget one when the next such page shows up.
    ///
    /// "Pinned" is now half a misnomer — debloat and godmode ride in the scrolling list
    /// rather than in the bottom strip — but what every caller of this actually asks is
    /// "is this a page the catalogue cannot answer for", and that is still exactly the
    /// set. Renaming it would touch more than it would explain.
    static bool isPinnedPage(const QString &id)
    {
        return id == settingsId() || id == actionsId() || id == debloatId()
               || id == cleanerId() || id == godModeId() || id == officeId()
               || id == journalId() || id == tiLauncherId() || id == appsId()
               || id == featuresId() || id == aboutId();
    }

    /// …and who built it, which is not a category, a setting, or anything you would come
    /// back to twice — it lives last, past everything you would actually use daily.
    static QString aboutId() { return QStringLiteral("about"); }

    void setSelected(const QString &categoryId);

    /// Sets the trailing count on a list row. Catalogue rows get theirs at build time from
    /// the catalogue itself; the app row cannot, because its number is only known once a
    /// live machine scan has come back.
    void setCategoryCount(const QString &categoryId, const QString &text);

Q_SIGNALS:
    void categoryActivated(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void buildList();
    void retranslateGroups();
    qreal aboutRowTop() const;
    qreal settingsRowTop() const;
    qreal tiLauncherRowTop() const;
    qreal actionsRowTop() const;
    qreal journalRowTop() const;
    int bottomBlockHeight() const;

    SearchField *m_search = nullptr;
    SmoothScrollArea *m_listScroll = nullptr;
    QWidget *m_list = nullptr;
    CategoryRow *m_settings = nullptr;
    CategoryRow *m_actions = nullptr;
    CategoryRow *m_tiLauncher = nullptr;
    CategoryRow *m_journal = nullptr;
    CategoryRow *m_about = nullptr;
    QVector<CategoryRow *> m_rows;          ///< every selectable category row, any order
    QVector<QWidget *> m_listItems;         ///< rows and group headers, top-to-bottom
    QVector<QPair<SectionHeader *, QString>> m_groupHeaders;  ///< header + its i18n key
};
