// themeswitch.h — picking the palette by looking at it.
//
// Eight cards, each a miniature of the window painted in the palette it stands for: title
// bar, sidebar, a couple of rows, the accent. The one in use carries an accent ring, the
// same language the sidebar uses for the selected category.
//
// This replaces a Koyu | Açık segmented control. The control was accurate and told you
// nothing; the preview is the setting.
//
// The cards wrap through Css::flexColumns like the two chip grids beside them, so the
// shape follows the width the widget is handed rather than a hard-coded column count: the
// settings page's full content column shows all eight in one row, and the narrower one in
// the setup wizard falls back to 4×2. The card itself is a fixed 84×52 picture at every
// interface scale, for the same reason ToggleSwitch is a fixed 30×16 — in this design the
// type scales and the drawn controls do not.

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
    /// eight cards carry no per-card state, so there is nothing to keep in sync.
    int columns() const;
    QRectF cardRect(int index) const;
    int indexAt(const QPointF &pos) const;
    void paintPreview(QPainter *p, const QRectF &box, Theme::Appearance appearance) const;

    int m_hovered = -1;
    int m_pressed = -1;
};
