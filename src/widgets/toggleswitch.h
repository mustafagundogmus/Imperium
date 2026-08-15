// toggleswitch.h — the 26×15 pill from §4 of the handoff.
//
//   off : background #232329, border 1px #33333A, 9px knob #8A8A93 at left 2px
//   on  : background + border = accent,            9px knob #141414   at left 13px
//   transition: background .15s, border-color .15s, left .15s
//
// The knob colour is deliberately *not* interpolated — the mockup only transitions
// background, border-color and left, so the knob swaps instantly.

#pragma once

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
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    bool m_checked = false;
    bool m_pressed = false;
    qreal m_t = 0.0;                 ///< 0 = off, 1 = on
    QVariantAnimation *m_anim = nullptr;
};
