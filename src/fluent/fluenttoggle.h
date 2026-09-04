// fluenttoggle.h — the Fluent ToggleSwitch from the handoff's "Kontroller".
//
//   40×20 · radius 10 · state label "Açık/Kapalı" 12px textSec to its left, right-aligned
//   in a column as wide as the wider of the two words (never under the handoff's 36px),
//   10px before the track
//   on   : track and border accent · knob 12px onAccent at left 23
//   off  : transparent track · 1px toggleOffBorder · knob knobOff at left 3
//   150ms between the two
//
// The label is part of the widget rather than the row's business, because the two are
// one control in the design: clicking the word flips the switch as well.
//
// The label column is measured rather than fixed because the handoff's 36px was measured
// for "On"/"Off": "Kapalı" in Segoe UI is wider than that and lost its K to the left edge,
// and "Desactivado" would lose half the word. Both words are measured, so the control
// keeps one width whichever way it is flipped, and it measures again when the language
// or the typeface changes.

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
    static constexpr int MinLabelWidth = 36;
    static constexpr int LabelGap = 10;

    /// The label column: the wider of the two state words in the current language and
    /// typeface, whole pixels, never under MinLabelWidth.
    static int labelWidth();

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
