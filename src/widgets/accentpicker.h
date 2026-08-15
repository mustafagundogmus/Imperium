// accentpicker.h — the four accent presets from the handoff, as a strip of swatches.
//
// 14px circles, 10px apart. The active one carries a 1px ring in its own colour, offset
// by 3px, which is the same visual language the sidebar uses for the selected category.

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
