// splashscreen.h — the first second and a half.
//
// The app has real work to do before its window can appear: the catalogue, 329 services,
// the startup entries, a machine's worth of system facts. That used to be a second of
// nothing on screen. This is that second, drawn.
//
// Nothing here is decoration for its own sake — it is the app's own language rehearsed:
// the diamond mark strokes itself on, the wordmark arrives with its letter-spacing
// tightening into place, a hairline sweeps the way the window's border light does, and
// the whole card fades into the real window. Same palette, same accent, same 1px rules.
//
// The animation and the loading overlap: the splash paints, the main window is built
// behind it, and finish() waits for whichever of the two is still going.

#pragma once

#include <QElapsedTimer>
#include <QWidget>

class QTimer;

class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);

    /// Runs the animation out, fades the card, then shows \a window. Returns once the
    /// window is up, so main() can go straight into the event loop after it.
    void finish(QWidget *window);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    /// 0…1 across the whole sequence.
    qreal progress() const;

    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
    qreal m_fade = 1.0;   ///< 1 while the card is up, eased to 0 by finish()
};
