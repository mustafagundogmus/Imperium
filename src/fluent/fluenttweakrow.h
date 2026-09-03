// fluenttweakrow.h — one tweak line of the Fluent shell (§4 of the Fluent handoff).
//
//   padding 12 16 · gap 16 · hover rowHover
//   icon box 32×32 radius 4 iconBg, the category's glyph at 16px in textSec
//   name 14px/500 text, then the badges: BEKLİYOR (accentSoft / accentText) while the
//   position differs from the machine's, BU YAPIDA YOK (track / textMuted) on a row this
//   build ignores — that row also drops to 55% — and, since the catalogue says so, the
//   risk word in amber or red
//   description 12px textSec, one line, elided
//   the control at the right: the Fluent toggle with its state word, the slider, or the
//   segmented control for a choice
//
// Same contract as TweakRow: the state lives in AppState and the row only draws it.

#pragma once

#include <QWidget>

class AppState;
class FluentSlider;
class FluentToggle;
class SegmentedControl;
struct Tweak;
namespace Icons { struct Glyph; }

class FluentTweakRow : public QWidget
{
    Q_OBJECT

public:
    /// \a categoryId is the owning category, the fallback for the row's glyph.
    FluentTweakRow(const Tweak &tweak, AppState *state, const QString &categoryId,
                   QWidget *parent = nullptr);

    /// The card clips its corners; the first and last rows have to clip their hover fill
    /// the same way or a square pixel shows at each corner.
    void setEdges(bool first, bool last);

    static int rowHeight();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    void positionControl();
    QWidget *control() const;

    QString m_id;
    QString m_name;
    QString m_desc;
    QString m_risk;
    const Icons::Glyph *m_glyph = nullptr;
    bool m_applicable = true;
    bool m_locked = false;
    QString m_blockReason;
    bool m_hovered = false;
    bool m_first = false;
    bool m_last = false;
    FluentToggle *m_toggle = nullptr;
    FluentSlider *m_slider = nullptr;
    SegmentedControl *m_segments = nullptr;
    AppState *m_state = nullptr;
};
