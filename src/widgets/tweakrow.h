// tweakrow.h — one tweak line from §4 of the handoff.
//
//   grid 26px | 1fr · gap 12px · padding 7px 6px (compact: 4px 6px)
//   border 1px transparent · radius 5px
//   name  12.5px #E8E8EA weight 450, wrapped at the column's width
//   desc  10.5px #77777F, line-height 1.45, wrapped at the column's width; the row is as
//         tall as the lines the two take
//   a choice's control at the end of the row — or under the text, when the column
//   beside the control would be too narrow to read
//
// The mockup gives the row `cursor:default` and hangs the click handler on the pill
// alone, so the row itself is inert and has no hover state.
//
// The row's height depends on its width — a description that fits one line at 900px
// takes two at 600 — so it is answered through heightForWidth rather than fixed at
// construction. Every QVBoxLayout between here and the scroll area passes that question
// up, and the scroll area sizes the page by the answer. The fixed height of before had
// to cut the description with an ellipsis, which hid the end of most of them.

#pragma once

#include <QColor>
#include <QStringList>
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

    /// The row at the width the layout offers: padding, the name's and the description's
    /// lines at that width — or the control beside them, when it is taller — plus the
    /// control below them, on a row narrow enough to put it there.
    int heightForWidth(int width) const override;

    /// The single-line row: what a layout with no width to offer gets.
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    /// Where the text sits in a row \a width wide, and how the name and the description
    /// fall there.
    struct TextLayout
    {
        qreal x = 0.0;            ///< left edge of the text column
        qreal width = 0.0;        ///< width of the text column
        bool stacked = false;     ///< the control is under the text rather than beside it
        QStringList nameLines;    ///< the name wrapped at \a width
        QString badge;            ///< the risk word and its separator; empty when not drawn
        qreal badgeWidth = 0.0;   ///< advance of \a badge, 0 when it is not drawn
        QColor badgeColor;
        QStringList descLines;    ///< the description (or requirement) wrapped at \a width
        qreal blockHeight = 0.0;  ///< name lines, the gap, description lines
        qreal controlAvail = 0.0; ///< width the control may take under the text; 0 beside it
        qreal controlH = 0.0;     ///< the control's height in the place it gets
    };
    TextLayout measure(int width) const;
    QWidget *trailing() const;
    void positionToggle();

    QString m_id;
    QString m_name;
    QString m_desc;
    QString m_risk;                          ///< "cost" | "unsafe" | empty; see Tweak::risk
    bool m_choice = false;
    bool m_applicable = true;
    QString m_requirement;
    ToggleSwitch *m_toggle = nullptr;        ///< set for a switch
    SegmentedControl *m_segments = nullptr;  ///< set for a choice
    RangeSlider *m_slider = nullptr;         ///< set for a range
    AppState *m_state = nullptr;
};
