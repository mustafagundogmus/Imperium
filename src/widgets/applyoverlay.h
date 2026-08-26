// applyoverlay.h — what "Uygula" looks like while it is happening.
//
// A scrim over the content area with a single card on top: a progress rail, a live
// counter, and the name and registry path of the tweak being written at that instant.
// When it finishes the same card turns into the result, and — if any of the tweaks that
// landed only take effect once the shell re-reads them — offers to restart Explorer.
//
// The apply itself is driven from here, one tweak per timer tick. The work is genuinely
// incremental; the pacing exists so the run is legible rather than a single frame's
// flash, and it is scaled so the whole thing takes about a second no matter how many
// tweaks are queued.

#pragma once

#include <QVector>
#include <QWidget>

class AppState;
class PillButton;
class QTimer;
class QVariantAnimation;
struct Tweak;

class ApplyOverlay : public QWidget
{
    Q_OBJECT

public:
    ApplyOverlay(AppState *state, QWidget *parent = nullptr);

    /// Shows the overlay and starts writing. Does nothing when nothing is pending.
    void run();

    bool running() const { return m_running; }

Q_SIGNALS:
    /// \a firstError is the message from the first tweak that failed, empty when none
    /// did. Without it MainWindow had nothing to put in its detailed failure notice, so
    /// the only thing a failed apply could ever say was how many had failed.
    void finished(int succeeded, int failed, bool elevationRequired, const QString &firstError);
    void notice(const QString &text);

    /// Emitted whenever the overlay comes up or goes away. MainWindow uses it to stand
    /// its Escape shortcut down: a window shortcut consumes the key before any widget
    /// receives a key press, so while that shortcut is live the overlay cannot be
    /// dismissed from the keyboard at all.
    void visibilityChanged(bool visible);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    void start();
    void step();
    void complete();
    void layoutButtons();
    QRectF cardRect() const;

    AppState *m_state = nullptr;
    QTimer *m_timer = nullptr;
    QVariantAnimation *m_fade = nullptr;
    QVariantAnimation *m_progress = nullptr;
    QVariantAnimation *m_shimmer = nullptr;

    QVector<QString> m_queue;      ///< tweak ids still to write
    int m_total = 0;
    int m_done = 0;
    int m_succeeded = 0;
    int m_failed = 0;
    bool m_elevationRequired = false;
    bool m_needsExplorer = false;
    QString m_firstError;

    QString m_currentName;
    QString m_currentPath;
    QString m_summary;

    qreal m_opacity = 0.0;
    qreal m_shown = 0.0;           ///< eased progress actually drawn
    qreal m_shimmerPos = 0.0;
    bool m_running = false;
    bool m_complete = false;

    PillButton *m_restartExplorer = nullptr;
    PillButton *m_close = nullptr;
};
