// rangeslider.h — the control for a tweak that is a number, not a state.
//
// Windows has plenty of these: a menu delay in milliseconds, a keyboard repeat delay in
// steps, lines per wheel notch. Asking "200 or 400?" with a switch is a lie about what
// the value is, and a segmented control with nine options is unreadable.
//
// The stops are discrete because the catalogue's positions are: the slider moves between
// the options a range tweak was expanded into, so everything downstream — pending changes,
// Uygula, presets, the .reg export — sees the same index it sees for every other tweak.

#pragma once

#include <QStringList>
#include <QWidget>

class RangeSlider : public QWidget
{
    Q_OBJECT

public:
    /// \a labels is one per stop, already formatted with its unit.
    RangeSlider(const QStringList &labels, QWidget *parent = nullptr);

    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);

    QSize sizeHint() const override;

Q_SIGNALS:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    qreal trackWidth() const;
    qreal knobX(int index) const;
    int indexAt(qreal x) const;

    QStringList m_labels;
    int m_current = 0;
    bool m_hovered = false;
    bool m_dragging = false;
};
