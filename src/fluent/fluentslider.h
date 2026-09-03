// fluentslider.h — the handoff's slider, for a range tweak under the Fluent shell.
//
//   220px in all: the rail, a 12px gap, a 44px right-aligned mono value ("40 gün")
//   rail 4px `track`, the travelled part `accent`
//   knob 20px circle, `card` fill, 1px controlBorder, shadow 0 1px 3px rgba(0,0,0,.2),
//   a 10px accent dot at its centre
//
// Discrete stops, like RangeSlider: the catalogue's positions are the stops and every
// consumer downstream sees an index.

#pragma once

#include <QStringList>
#include <QWidget>

class FluentSlider : public QWidget
{
    Q_OBJECT

public:
    FluentSlider(const QStringList &labels, QWidget *parent = nullptr);

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
    qreal railLeft() const;
    qreal railWidth() const;
    qreal knobX(int index) const;
    int indexAt(qreal x) const;

    QStringList m_labels;
    int m_current = 0;
    bool m_hovered = false;
    bool m_dragging = false;
};
