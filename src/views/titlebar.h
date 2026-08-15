// titlebar.h — §1 of the handoff.
//
//   36px tall, bottom border 1px #1D1D22, padding-left 12px
//   left  : 10px accent diamond · "tweaker" mono 11.5/500/.02em · "v0.9.2" mono 10 #55555E
//   right : system summary mono 10 #55555E with a 14px right margin, then three
//           40×36 window buttons (hover #1C1C21, close hover #3A1D1F)
//
// Dragging is handed to the compositor via QWindow::startSystemMove(), so Aero Snap and
// the drag-to-restore gesture keep working on a frameless window.

#pragma once

#include <QWidget>

class WindowButton;

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setSystemSummary(const QString &summary);
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
    QString m_summary;
    WindowButton *m_minimize = nullptr;
    WindowButton *m_maximize = nullptr;
    WindowButton *m_close = nullptr;
};
