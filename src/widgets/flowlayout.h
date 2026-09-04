// flowlayout.h — CSS `flex-wrap: wrap` as a QLayout.
//
// WinUtil lays the install tab out in a WrapPanel: tiles of one width flow left to right
// and break to the next line where the column runs out. Qt has no layout that does this
// — the grid needs a column count and the box layouts never wrap — so this is the
// classic flow layout, written for one job: put every child at its size hint, in order,
// wrapping at the layout's width, and report the height that takes so the page above
// can scroll it.

#pragma once

#include <QLayout>
#include <QVector>

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent, int hGap, int vGap);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;

    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;

private:
    /// Lays the items out inside \a rect and returns the height used. \a dryRun only
    /// measures.
    int doLayout(const QRect &rect, bool dryRun) const;

    QVector<QLayoutItem *> m_items;
    int m_hGap;
    int m_vGap;
};
