// tweakrow.h — one tweak line from §4 of the handoff.
//
//   grid 26px | 1fr · gap 12px · padding 7px 6px (compact: 4px 6px)
//   border 1px transparent · radius 5px
//   name  12.5px #E8E8EA weight 450
//   desc  10.5px #77777F, line-height 1.45, single line, ellipsised on overflow
//
// The mockup gives the row `cursor:default` and hangs the click handler on the pill
// alone, so the row itself is inert and has no hover state.

#pragma once

#include <QWidget>

class AppState;
class RangeSlider;
class SegmentedControl;
class ToggleSwitch;
struct Tweak;

class TweakRow : public QWidget
{
    Q_OBJECT

public:
    TweakRow(const Tweak &tweak, AppState *state, QWidget *parent = nullptr);

    /// Row height for the current compact setting, derived from the two line boxes —
    /// or from the segmented control, when the tweak is a choice and taller than them.
    static int rowHeight(bool trailingControl = false);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void positionToggle();

    QString m_id;
    QString m_name;
    QString m_desc;
    bool m_choice = false;
    bool m_applicable = true;
    QString m_requirement;
    ToggleSwitch *m_toggle = nullptr;        ///< set for a switch
    SegmentedControl *m_segments = nullptr;  ///< set for a choice
    RangeSlider *m_slider = nullptr;         ///< set for a range
    AppState *m_state = nullptr;
};
