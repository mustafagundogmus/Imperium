// borderglow.h — the light that walks the window's edge.
//
// A long arc of accent light sweeps clockwise around the card's 1px border: a bright
// head, then a tail that fades out over roughly two fifths of the perimeter, with a soft
// haze spilling a few pixels outward into the shadow margin and inward over the content.
// One lap takes about nine seconds.
//
// It is an overlay across the whole window instead of something FramelessWindow paints
// beneath its children: the title bar, the sidebar and the status bar all fill their own
// backgrounds, so a glow painted under them would lose its inward half — and when the
// window is maximised the shadow margin is gone and the inward half is all there is.
//
// The obvious way to sweep an arc around a rectangle is a conical gradient, which is what
// most of these effects use. It is wrong on a rectangle: equal angles are unequal
// distances, so on a 1240×760 window the head crawls along the middle of an edge and
// bolts around the corners — a 3.7× swing — while the tail stretches and shrinks. So the
// track is walked by arc length instead. The tail is cut into runs at the corners, each
// run drawn with a linear gradient carrying that stretch of the fade, and the two runs
// meeting at a corner are clipped against the mitre bisector so they tile it exactly
// rather than overlapping into a bright knot.
//
// Across the border the fade is a stack of concentric strokes at a low alpha each: a
// single wide pen would draw a band with a visible lip down either side.

#pragma once

#include <QElapsedTimer>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QRegion>
#include <QVector>
#include <QWidget>

class QTimer;

class BorderGlow : public QWidget
{
    Q_OBJECT

public:
    explicit BorderGlow(QWidget *parent = nullptr);

    /// The line the light runs along — the card's border — and its corner radius.
    void setTrack(const QRectF &track, qreal radius);

    /// The glow fades out and stops while the window is not the active one.
    void setActive(bool active);

    /// Stops the clock entirely — used while the window is minimised.
    void setSuspended(bool suspended);

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    /// One straight stretch of the tail: everything between two corners of the track.
    struct Run
    {
        QPointF from;       ///< drawn from — pushed past the corner when it joins another
        QPointF to;
        QPointF gradFrom;   ///< where the fade really starts and ends, for the gradient
        QPointF gradTo;
        qreal dFrom = 0.0;  ///< distance behind the head at gradFrom …
        qreal dTo = 0.0;    ///< … and at gradTo, negative ahead of it
        QPainterPath clipFrom;   ///< mitre half-planes; empty when the end is free
        QPainterPath clipTo;
    };

    void tick();
    void setRunning(bool run);
    void rebuild();
    int segmentAt(qreal distance) const;
    QVector<Run> buildRuns(qreal tail) const;
    QRegion regionFor(const QVector<Run> &runs) const;
    qreal tailLength() const;

    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;

    QRectF m_track;
    qreal m_radius = 0.0;
    QPolygonF m_points;      ///< the track, flattened and closed
    QVector<qreal> m_cumulative;   ///< arc length at each point
    qreal m_length = 0.0;    ///< perimeter of the track
    QRegion m_band;          ///< the ring the glow can reach, i.e. all it may repaint

    qreal m_distance = 0.0;  ///< where the head is, along the perimeter
    qreal m_step = 0.0;      ///< how far it moved last frame, for the repaint region
    qreal m_phase = 0.0;     ///< 0…1 of the breathing cycle
    qreal m_level = 1.0;     ///< eased in on focus, out on losing it
    bool m_active = true;
    bool m_suspended = false;
};
