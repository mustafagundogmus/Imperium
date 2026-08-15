// toggleswitch.h — the switch used everywhere in the app.
//
// The handoff draws a flat 26×15 pill that cross-fades to the accent. This is a richer
// take on the same idea, kept inside the design's language (no gradients, no glass, 1px
// borders only):
//
//   · the accent does not cross-fade in — it *wipes* across the track, following the
//     knob, so the fill and the knob read as one movement
//   · the knob squashes while held and springs back on release
//   · turning on overshoots very slightly (OutBack), turning off settles (OutCubic)
//   · a soft accent halo sits under the track while on, which is what gives it presence
//     against #121214 without adding a single new colour

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
    QRectF capsule() const;        ///< the drawn switch inside the padded widget
    qreal knobCentre() const;      ///< x of the knob centre at the current position
    qreal knobRadius() const;
    void animateTo(qreal target, bool overshoot);

    bool m_checked = false;
    bool m_pressed = false;
    bool m_hovered = false;

    qreal m_t = 0.0;               ///< 0 = off, 1 = on (may briefly exceed 1 on overshoot)
    qreal m_squash = 0.0;          ///< 0 = round knob, 1 = fully squashed
    QVariantAnimation *m_slide = nullptr;
    QVariantAnimation *m_squashAnim = nullptr;
};
