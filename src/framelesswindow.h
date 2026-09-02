// framelesswindow.h — the window shell.
//
//   1060×820 card · radius 9px · border 1px #26262B · background #121214
//   drop shadow, painted by the window itself into a transparent margin
//
// Moving and resizing are delegated to the compositor through QWindow::startSystemMove()
// and startSystemResize(), so Aero Snap, edge-drag maximise and the double-click gesture
// all behave natively even though the frame is ours.
//
// The shadow margin collapses to zero when the window is maximised, and also when the
// screen is too short to afford it — an 820px card plus a literal 80px CSS blur does not
// fit on a 960pt-tall desktop.
//
// A BorderGlow overlay rides on top of everything and walks a soft light around that
// border; it is switched from the settings page and is purely decorative.

#pragma once

#include <QPixmap>
#include <QWidget>

class BorderGlow;

class FramelessWindow : public QWidget
{
    Q_OBJECT

public:
    explicit FramelessWindow(QWidget *parent = nullptr);

    /// Parent for everything inside the card. Never null.
    QWidget *card() const { return m_card; }

    /// Minimum size of the card itself, excluding the shadow margin.
    void setCardMinimumSize(const QSize &size);

    void toggleMaximize();

    /// Renders just the card (no shadow) — used by the --screenshot switch.
    QPixmap grabCard();

Q_SIGNALS:
    void maximizedChanged(bool maximized);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void changeEvent(QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    bool event(QEvent *) override;
    bool eventFilter(QObject *watched, QEvent *e) override;

private:
    QMargins shadowMargins() const;
    QRect cardRect() const;

    /// The resize edges under  pos.  insideToo says whether the band just inside the
    /// card edge counts as well as the one over the shadow; see eventFilter() for when it
    /// must not.
    Qt::Edges edgeAt(const QPoint &pos, bool insideToo = true) const;
    void rebuildShadow();
    void layoutGlow();

    /// Sets the resize cursor for \a edges, or puts it back to the arrow when \a edges is
    /// empty. Driven from an application-wide event filter rather than this widget's own
    /// mouseMoveEvent: that only fires while the pointer is directly over FramelessWindow's
    /// own (mostly unclad) area, so the moment it crosses onto any child — a scrollbar, a
    /// row, a button — nothing here would otherwise run again to put the cursor back.
    void applyEdgeCursor(const QPoint &pos, bool insideToo = true);

    QWidget *m_card = nullptr;
    BorderGlow *m_glow = nullptr;
    QSize m_cardMinimum;
    QPixmap m_shadow;
    bool m_shadowEnabled = true;
};
