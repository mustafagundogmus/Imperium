// toggleswitch.h — the switch used everywhere in the app.
//
// Rectangular rather than a pill, to match the window's square corners and the 1px-line
// language of the rest of the UI:
//
//   track  30×16, 3px radius, 1px border
//   knob   10×10 square, 2px radius, inset 2px from the padding box
//   on     the accent fills the track *behind* the knob — the fill edge tracks the knob
//          instead of cross-fading, so the two read as one movement
//   motion 140ms, no overshoot: short and decided

#pragma once

#include <QRectF>
#include <QWidget>

class QVariantAnimation;

class ToggleSwitch : public QWidget
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool on, bool animate = true);

    QSize sizeHint() const override;

Q_SIGNALS:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    QRectF knobRect() const;

    bool m_checked = false;
    bool m_pressed = false;
    bool m_hovered = false;

    qreal m_t = 0.0;               ///< 0 = off, 1 = on
    QVariantAnimation *m_slide = nullptr;
};
