// sidebar.h — §2 of the handoff.
//
//   212px wide, right border 1px #1D1D22, padding 10px 8px 0
//   search field inset a further 2px, 10px of space below it
//   category rows 28px tall with a 1px gap
//   bottom block: top border 1px #1D1D22, padding 10px 8px 12px, 3px column gap
//
// Geometry is assigned by hand rather than by a QLayout: the design specifies the
// margins in CSS terms (a margin that collapses into the parent's padding) and hand
// placement keeps those numbers literal.

#pragma once

#include <QVector>
#include <QWidget>

class AppState;
class CategoryRow;
class LinkLabel;
class SearchField;

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(AppState *state, QWidget *parent = nullptr);

    SearchField *search() const { return m_search; }

    /// Category id used by the pinned Ayarlar row at the bottom of the list.
    static QString settingsId() { return QStringLiteral("settings"); }

    void setSelected(const QString &categoryId);
    void setRestorePoint(const QString &value);

Q_SIGNALS:
    void categoryActivated(const QString &id);
    void restorePointRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    static qreal restoreRowAscent();
    static qreal restoreRowHeight();
    qreal settingsRowTop() const;
    qreal restoreRowTop() const;
    qreal restoreLabelTop() const;
    int bottomBlockHeight() const;

    SearchField *m_search = nullptr;
    QWidget *m_list = nullptr;
    CategoryRow *m_settings = nullptr;
    QVector<CategoryRow *> m_rows;
    LinkLabel *m_link = nullptr;
    QString m_restorePoint = QStringLiteral("—");
};
