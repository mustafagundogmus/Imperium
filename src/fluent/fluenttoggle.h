// fluenttoggle.h — the Fluent ToggleSwitch from the handoff's "Kontroller".
//
//   40×20 · radius 10 · state label "Açık/Kapalı" 12px textSec to its left, in a 36px
//   column right-aligned, 10px before the track
//   on   : track and border accent · knob 12px onAccent at left 23
//   off  : transparent track · 1px toggleOffBorder · knob knobOff at left 3
//   150ms between the two
//
// The label is part of the widget rather than the row's business, because the two are
// one control in the design: clicking the word flips the switch as well.

#pragma once

#include <QWidget>

class QVariantAnimation;

class FluentToggle : public QWidget
{
    Q_OBJECT

public:
    explicit FluentToggle(QWidget *parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool on, bool animate = true);

    QSize sizeHint() const override;

    static constexpr int TrackWidth = 40;
    static constexpr int TrackHeight = 20;
    static constexpr int LabelWidth = 36;
    static constexpr int LabelGap = 10;

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
    bool m_checked = false;
    bool m_pressed = false;
    bool m_hovered = false;
    qreal m_t = 0.0;
    QVariantAnimation *m_slide = nullptr;
};
