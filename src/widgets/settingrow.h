// settingrow.h — one line in the settings page.
//
// Deliberately the same geometry as TweakRow (padding 7px 6px, 12px column gap, name at
// 12.5px over a 10.5px description) so the settings page reads as part of the same list
// system rather than as a separate dialog. Three arrangements:
//
//   Leading   [ 30px control ] gap 12 [ name / desc ]        — switches
//   Trailing  [ name / desc ................. ] gap 16 [ control ]  — choices, buttons
//   Below     [ name / desc, the whole column ]
//             [ control, the whole column ]                  — galleries
//
// Below is what the four pickers on the settings page needed. A gallery of eight theme
// cards or ten language chips is not a trailing control: squeezed into the right-hand end
// of a row it takes the width the description wanted, wraps into a ragged block, and drags
// a 40px row out to 160 — which is the "cramped and jumbled" the page was reported for. On
// its own line it gets the full column, wraps to a grid, and leaves the description the
// whole width as well; the language row's description is the longest string on the page
// and was the one being elided hardest.
//
// A Below row keeps twice the vertical padding of a list row, so the air between two
// blocks is visibly more than the air between a caption and the gallery it labels. That is
// the only thing on this page that separates one setting from the next — the rows carry no
// rule and no background — and at the single padding the two gaps came out 15 and 10,
// which is not a difference a reader can see.
//
// Its description may also run to a second line. Measured against the column it gets, the
// Dil caption is 1136px of French text at the default size and 1439 at the largest, so on
// a single line it is elided no matter how wide the row is. Leading and Trailing keep the
// one elided line the tweak list uses.

#pragma once

#include <QStringList>
#include <QWidget>

class SettingRow : public QWidget
{
    Q_OBJECT

public:
    enum Placement { Leading, Trailing, Below };

    SettingRow(const QString &name, const QString &desc, QWidget *control,
               Placement placement, QWidget *parent = nullptr);

    /// Rows with a taller control (a swatch strip, a two-button pair) grow to fit it.
    static int rowHeight();

    void setName(const QString &name);
    void setDesc(const QString &desc);

    QSize sizeHint() const override;
    /// Only a Below row's height depends on its width, and it depends on it twice over:
    /// the control gets the whole column, and a gallery that wraps reports a different
    /// height for every column count.
    bool hasHeightForWidth() const override { return m_placement == Below; }
    int heightForWidth(int width) const override { return heightFor(width); }

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void positionControl();
    /// Invalidates every layout between this row and its window. Only a Below row needs
    /// it, and only when the type metrics change under it; see the note in the .cpp.
    void invalidateLayoutChain();
    int heightFor(int width) const;
    /// Height the control asks for when it is given \a available px to lay out in.
    int controlHeightFor(int available) const;
    /// The description broken to \a textW. One line everywhere but Below, which may run to
    /// two — its captions are sentences, and the longest of them does not fit on one line
    /// even given the whole column. Cached: the break costs a text measurement per word and
    /// is wanted on every paint.
    const QStringList &descLines(qreal textW) const;
    qreal textBlock(qreal textW) const;
    qreal textWidthFor(int width) const;

    QString m_name;
    QString m_desc;
    QWidget *m_control = nullptr;
    Placement m_placement;

    mutable QStringList m_lines;         ///< descLines() cache
    mutable qreal m_linesWidth = -1.0;   ///< the column it was measured against
    mutable bool m_linesDirty = true;    ///< set when the text or the font changes
};
