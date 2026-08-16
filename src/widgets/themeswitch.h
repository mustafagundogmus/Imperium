// themeswitch.h — picking the palette by looking at it.
//
// Two cards side by side, each a miniature of the window painted in the palette it
// stands for: title bar, sidebar, a couple of rows, the accent. The one in use carries
// an accent ring, the same language the sidebar uses for the selected category.
//
// This replaces a Koyu | Açık segmented control. The control was accurate and told you
// nothing; the preview is the setting.

#pragma once

#include "../theme.h"

#include <QWidget>

class ThemeSwitch : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

Q_SIGNALS:
    void picked(Theme::Appearance appearance);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF cardRect(int index) const;
    int indexAt(const QPointF &pos) const;
    void paintPreview(QPainter *p, const QRectF &box, Theme::Appearance appearance) const;

    int m_hovered = -1;
    int m_pressed = -1;
};
