// fluenttitlebar.h — §1 of the Fluent handoff.
//
//   48px on mica, no bottom rule · gap 10
//   centre: logo 18×18 · "Arbitrium" 12px/500 · "v0.14.0" 11px textMuted — centred on
//           the window, not on the bar, which spans the content column only (the rail
//           and the pane took the left end of it; see fluentchrome.h)
//   right : the theme button (28px tall, 12px text, a 12px accent dot before it, hover
//           subtleHover, 8px before the controls), then three 46×48 window controls —
//           hover subtleHover, the close one #C42B1C with a white glyph
//
// Dragging and the double-click go to the compositor, as in TitleBar.

#pragma once

#include <QWidget>

class FluentWindowButton;
class FluentThemeButton;

class FluentTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit FluentTitleBar(QWidget *parent = nullptr);

    void setMaximized(bool maximized);

    QSize sizeHint() const override;

Q_SIGNALS:
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    FluentThemeButton *m_theme = nullptr;
    FluentWindowButton *m_minimize = nullptr;
    FluentWindowButton *m_maximize = nullptr;
    FluentWindowButton *m_close = nullptr;
};

/// The 46×48 window control. Its own class rather than WindowButton because every
/// measurement differs and the close state is the handoff's fixed red, not the accent.
class FluentWindowButton : public QWidget
{
    Q_OBJECT

public:
    enum Kind { Minimize, Maximize, Restore, Close };

    explicit FluentWindowButton(Kind kind, QWidget *parent = nullptr);

    void setKind(Kind kind);

    /// The window's corner radius to clip the hover fill by — only the close button, at
    /// the top-right corner, is ever told a non-zero value.
    void setCornerRadius(qreal radius);

Q_SIGNALS:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    Kind m_kind;
    qreal m_corner = 0.0;
    bool m_hovered = false;
    bool m_pressed = false;
};

/// "● Koyu" / "● Açık": flips between the dark and light family.
class FluentThemeButton : public QWidget
{
    Q_OBJECT

public:
    explicit FluentThemeButton(QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QString label() const;
    void refreshGeometry();

    bool m_hovered = false;
    bool m_pressed = false;
};
