// themeswitch.h — picking the palette by looking at it.
//
// Twelve cards, each a miniature of the window painted in the palette it stands for: title
// bar, sidebar, a couple of rows, the accent. The one in use carries an accent ring, the
// same language the sidebar uses for the selected category.
//
// This replaces a Koyu | Açık segmented control. The control was accurate and told you
// nothing; the preview is the setting.
//
// The cards wrap through Css::flexColumns like the two chip grids beside them, so the
// shape follows the width the widget is handed rather than a hard-coded column count. At
// twelve that is 6×2 in both of the places this appears, which is the tidiest shape the
// count admits and the reason four were added rather than three:
//
//   settings page   the content column is 970px at the window's 1240px minimum — a row of
//                   84px cards and 10px gaps fits 10, so flexColumns takes the two rows
//                   that implies and rebalances them to 6 and 6 rather than 10 and 2
//   setup wizard    the card is a fixed 840px, less 2px of border and 2×40px of page
//                   padding, so the widget is handed 758 and fits 8 — two rows again, and
//                   rebalanced to the same 6 and 6
//
// Twelve also never leaves a ragged last row at any width the widget can be handed, not
// just at those two. As the width comes down flexColumns lands on 12, 6, 4, 3, 2 and 1
// columns and nothing in between, and twelve divides evenly by every one of them; eleven
// would have been ragged at four of its six shapes. That is the argument for four more
// rather than three.
//
// Both are independent of the interface scale: the column count is decided by CardW and
// Gap, and the card itself is a fixed 84×52 picture at every text size, for the same
// reason ToggleSwitch is a fixed 30×16 — in this design the type scales and the drawn
// controls do not. What the four text sizes do change is the label line under each card,
// and so the height of a row; heightForWidth() is measured from Font::tileSub() for that
// reason rather than from a constant.

#pragma once

#include "../theme.h"

#include <QWidget>

class ThemeSwitch : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

Q_SIGNALS:
    void picked(Theme::Appearance appearance);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    /// Columns at the width the widget currently has. Derived rather than stored: the
    /// twelve cards carry no per-card state, so there is nothing to keep in sync.
    int columns() const;
    QRectF cardRect(int index) const;
    int indexAt(const QPointF &pos) const;
    void paintPreview(QPainter *p, const QRectF &box, Theme::Appearance appearance) const;

    int m_hovered = -1;
    int m_pressed = -1;
};
