// segmentedcontrol.h — the "Tümü | Etkin | Değişen" filter from §3 of the handoff.
//
//   border 1px #26262C · radius 5px · overflow hidden
//   segment padding 3px 10px, 11px
//   active   background #1C1C21, text #E8E8EA
//   inactive text #7A7A84, hover #C6C6CE, separated by a 1px #26262C rule
//
// One line by nature. Told a width it may not exceed (setAvailableWidth), it breaks the
// segments into as many lines as they need, each line its own bordered group 4px under
// the one before — the way a four-way choice with sentence-long labels has to fit under
// a tweak's text in a narrow window. A single segment wider than that width is drawn at
// the width with its label elided, so nothing ever runs past the edge.

#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>

class SegmentedControl : public QWidget
{
    Q_OBJECT

public:
    explicit SegmentedControl(const QStringList &labels, QWidget *parent = nullptr);

    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);

    /// Same segment count and order, new text — for a language switch. Reshapes and
    /// repaints; the selection and hover state are left alone.
    void setLabels(const QStringList &labels);

    /// The natural control: every segment on one line, whatever width it was given.
    QSize sizeHint() const override;

    /// Lays the control out no wider than \a width — see the header — and sizes itself
    /// to the result. Nothing, the default, is the natural line again.
    void setAvailableWidth(qreal width);

    /// The height setAvailableWidth(\a width) would leave, without laying anything out —
    /// a row answering heightForWidth needs it before it can move the control.
    int heightForWidth(int width) const override;

    /// Height of one line at the current font, before a control exists — a row that has
    /// to reserve space for one needs the number without building it.
    static qreal controlHeight();

Q_SIGNALS:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    /// A run of segments sharing one bordered line.
    struct Line
    {
        int first = 0;      ///< index of its first segment
        int count = 0;
        qreal width = 0.0;  ///< both borders and the rules between segments included
    };
    /// The segments broken into lines no wider than \a available (0: one line).
    QVector<Line> lines(qreal available) const;
    /// A segment's width, padding included, capped to \a available when that is set.
    qreal segmentWidth(int index, qreal available) const;
    /// Where the segment sits in the widget as laid out now.
    QRectF segmentRect(int index) const;
    int segmentAt(const QPointF &pos) const;
    void relayout();

    QStringList m_labels;
    int m_current = 0;
    int m_hovered = -1;
    qreal m_available = 0.0;   ///< the width given to setAvailableWidth; 0 is the natural line
};
