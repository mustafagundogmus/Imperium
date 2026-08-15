// segmentedcontrol.h — the "Tümü | Etkin | Değişen" filter from §3 of the handoff.
//
//   border 1px #26262C · radius 5px · overflow hidden
//   segment padding 3px 10px, 11px
//   active   background #1C1C21, text #E8E8EA
//   inactive text #7A7A84, hover #C6C6CE, separated by a 1px #26262C rule

#pragma once

#include <QStringList>
#include <QWidget>

class SegmentedControl : public QWidget
{
    Q_OBJECT

public:
    explicit SegmentedControl(const QStringList &labels, QWidget *parent = nullptr);

    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);

    QSize sizeHint() const override;

Q_SIGNALS:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    qreal segmentWidth(int index) const;
    int segmentAt(const QPointF &pos) const;

    QStringList m_labels;
    int m_current = 0;
    int m_hovered = -1;
};
