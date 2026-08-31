// accentpicker.h — the accent presets from the handoff, as a strip of swatches.
//
// 14px circles, 10px apart — the same gap the theme cards and the chip grids keep. The
// active one carries a 1px ring in its own colour, offset by 3px, which is the same visual
// language the sidebar uses for the selected category.
//
// The dot is a fixed size at every interface scale, like ToggleSwitch's 30×16 capsule: in
// this design the type scales and the drawn controls do not. What does follow the type is
// the strip's height, which is the app's one pill height — so the band you can click grows
// with the text size even though the dot inside it does not, and the strip sits on the same
// rhythm as the segmented control and the chip grids stacked above it.
//
// No wrapping here, unlike the three galleries beside it: eight 14px dots come to 188px,
// which is narrower than the settings column at any text size and any window width.

#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class AccentPicker : public QWidget
{
    Q_OBJECT

public:
    explicit AccentPicker(QWidget *parent = nullptr);

    QSize sizeHint() const override;

Q_SIGNALS:
    void picked(const QColor &colour);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    int indexAt(const QPointF &pos) const;

    QVector<QColor> m_colours;
    int m_hovered = -1;
};
