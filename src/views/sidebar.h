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
// others with nothing to say which of them are related. Grouped into five clusters with
// a small header over each, the same list reads instead of just being scanned. Genel
// Bakış stays outside every cluster: it is the one row that is not a settings area, it
// is the dashboard, and grouping it with anything else would say it belongs to a
// category the way the others do.
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
class LinkLabel;
class SearchField;
class SectionHeader;

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

    /// …and the write history, which is not a category either.
    static QString journalId() { return QStringLiteral("journal"); }

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
    qreal actionsRowTop() const;
    qreal journalRowTop() const;
    int bottomBlockHeight() const;

    SearchField *m_search = nullptr;
    QWidget *m_list = nullptr;
    CategoryRow *m_settings = nullptr;
    CategoryRow *m_actions = nullptr;
    CategoryRow *m_journal = nullptr;
    CategoryRow *m_about = nullptr;
    QVector<CategoryRow *> m_rows;          ///< every selectable category row, any order
    QVector<QWidget *> m_listItems;         ///< rows and group headers, top-to-bottom
    QVector<QPair<SectionHeader *, QString>> m_groupHeaders;  ///< header + its i18n key
};
