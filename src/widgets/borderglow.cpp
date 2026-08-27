#include "borderglow.h"

#include "../theme.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QtMath>

#include <cmath>

namespace {

// 25fps. Every frame repaints a strip of the window under the tail, and everything that
// strip touches — the title bar, the sidebar, the status bar — repaints with it, which
// costs more than the glow's own painting does. At this speed the head advances 10px a
// frame, well inside its own lead-in, so nothing about the motion looks stepped.
constexpr int   FrameMs       = 30;
constexpr qreal Speed         = 260.0;   // px/s: ~15s a lap on the default 1240×760 card
constexpr qreal TailShare     = 0.15;    // of the perimeter, the arc the light stretches over
constexpr qreal LeadIn        = 14.0;    // so the head is a bright edge, not a razor cut
constexpr qreal BreathSeconds = 3.4;     // the "hafifçe parıldama": ±18% of intensity
constexpr qreal FadeSeconds   = 0.55;    // fade out when the window loses focus

// Across the border: concentric strokes at a low alpha each. Where n of them overlap the
// alpha is 1-(1-a)^n, and since the widths step down evenly, n falls off linearly with
// the distance from the line — a soft edge for the price of a few extra strokes.
constexpr int   HaloBands = 1;
constexpr qreal HaloWidth = 2.0;     // widest pen: the haze reaches 1px to either side
constexpr qreal HaloAlpha = 0.07;    // per band, before the stacking

// The lit border line itself, riding on top of the haze. It carries the effect — the
// haze is only there to seat it — so it stays the sharpest thing on screen, and short of
// opaque: a light passing over the border, not a second border in the accent colour.
constexpr qreal CoreWidth = 1.0;   // the border's own width, so the light sits in it
constexpr qreal CoreAlpha = 0.85;

/// Around a run: half the widest pen, and a pixel for the antialiasing.
constexpr qreal Pad = HaloWidth * 0.5 + 1.5;
/// Around the whole track: the same, plus the mitre pushing the haze √2/2 further out.
constexpr qreal Bleed = HaloWidth * 0.71 + 2.0;

/// Alpha along the tail, \a d being the distance behind the head. The shape is the one
/// the reference conical gradient used: full at the head, a little over half of that at
/// 45% of the tail, gone at the end of it.
qreal ramp(qreal d, qreal tail)
{
    if (d < 0.0)                       // the short lead-in ahead of the head
        return d < -LeadIn ? 0.0 : 1.0 + d / LeadIn;
    if (d >= tail)
        return 0.0;

    const qreal u = d / tail;
    return u < 0.45 ? 1.0 - 0.45 * (u / 0.45)
                    : 0.55 * (1.0 - (u - 0.45) / 0.55);
}

qreal wrap(qreal value, qreal span)
{
    if (span <= 0.0)
        return 0.0;
    const qreal r = std::fmod(value, span);
    return r < 0.0 ? r + span : r;
}

QPointF unit(const QPointF &v)
{
    const qreal n = std::hypot(v.x(), v.y());
    return n > 0.0001 ? v / n : QPointF(1.0, 0.0);
}

/// Everything on the far side of the line through \a c with normal \a n, as a polygon big
/// enough to cover any window. Two of these, back to back, tile the plane exactly — the
/// line is shared, so what one of them leaves uncovered the other one covers. Biasing
/// them apart to be safe would be worse than the gap it avoids: the overlap doubles the
/// alpha along the seam and draws a diagonal scratch across the corner.
QPainterPath halfPlane(const QPointF &c, const QPointF &n, qreal extent)
{
    const QPointF along(-n.y(), n.x());
    QPolygonF poly;
    poly << c + along * extent
         << c - along * extent
         << c - along * extent - n * extent
         << c + along * extent - n * extent;

    QPainterPath path;
    path.addPolygon(poly);
    return path;
}

} // namespace

BorderGlow::BorderGlow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::NoFocus);

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(FrameMs);
    connect(m_timer, &QTimer::timeout, this, &BorderGlow::tick);

    // The light is the accent, so both tokens move it.
    const auto repaintAll = [this] { update(m_band); };
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, repaintAll);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, repaintAll);
}

void BorderGlow::setTrack(const QRectF &track, qreal radius)
{
    if (track == m_track && qFuzzyCompare(radius + 1.0, m_radius + 1.0))
        return;
    m_track = track;
    m_radius = radius;
    rebuild();
    update();
}

void BorderGlow::rebuild()
{
    m_points.clear();
    m_cumulative.clear();
    m_length = 0.0;
    m_band = QRegion();

    if (m_track.width() < 8.0 || m_track.height() < 8.0)
        return;

    QPainterPath path;
    if (m_radius > 0.0)
        path.addRoundedRect(m_track, m_radius, m_radius);
    else
        path.addRect(m_track);

    // Flattened once here so the sweep can walk it by length. With square corners this
    // is the four sides; with a radius, the corner arcs come back as short chords and
    // simply turn into more runs.
    const QList<QPolygonF> subpaths = path.toSubpathPolygons();
    if (subpaths.isEmpty())
        return;
    m_points = subpaths.first();
    if (m_points.size() < 4)
        return;
    if (m_points.first() != m_points.last())
        m_points.append(m_points.first());

    m_cumulative.resize(m_points.size());
    m_cumulative[0] = 0.0;
    for (int i = 1; i < m_points.size(); ++i) {
        const QPointF d = m_points[i] - m_points[i - 1];
        m_cumulative[i] = m_cumulative[i - 1] + std::hypot(d.x(), d.y());
    }
    m_length = m_cumulative.last();
    m_distance = wrap(m_distance, m_length);

    const QRect outer = m_track.adjusted(-Bleed, -Bleed, Bleed, Bleed).toAlignedRect();
    const QRect inner = m_track.adjusted(Bleed, Bleed, -Bleed, -Bleed).toAlignedRect();
    m_band = QRegion(outer) - QRegion(inner);
}

qreal BorderGlow::tailLength() const
{
    return m_length * TailShare;
}

int BorderGlow::segmentAt(qreal distance) const
{
    // The last entry is the closing point, so there is one fewer segment than points.
    const int segments = m_points.size() - 1;
    for (int i = segments - 1; i >= 0; --i)
        if (distance >= m_cumulative[i])
            return i;
    return 0;
}

QVector<BorderGlow::Run> BorderGlow::buildRuns(qreal tail) const
{
    QVector<Run> runs;
    const int segments = m_points.size() - 1;
    if (segments < 3 || m_length <= 1.0)
        return runs;

    // Walked backwards from the tip of the lead-in, which is ahead of the head.
    const qreal span = qMin(LeadIn + tail, m_length - 1.0);
    const qreal tip = wrap(m_distance + LeadIn, m_length);

    int index = segmentAt(tip);
    qreal into = tip - m_cumulative[index];   // how far into that segment the tip sits
    qreal covered = 0.0;

    while (covered < span - 0.01 && runs.size() < 64) {
        const qreal take = qMin(into, span - covered);
        if (take > 0.01) {
            const QPointF direction = unit(m_points[index + 1] - m_points[index]);
            Run run;
            run.gradTo = m_points[index] + direction * into;
            run.gradFrom = m_points[index] + direction * (into - take);
            run.dTo = covered - LeadIn;
            run.dFrom = covered + take - LeadIn;
            run.from = run.gradFrom;
            run.to = run.gradTo;
            runs.append(run);
        }
        covered += take;
        index = (index + segments - 1) % segments;
        into = m_cumulative[index + 1] - m_cumulative[index];
    }

    // Where two runs meet at a corner, push both a little past it and split the overlap
    // along the mitre bisector: together they cover the corner once, and exactly.
    for (int i = 0; i + 1 < runs.size(); ++i) {
        Run &near = runs[i];         // the one closer to the head
        Run &far = runs[i + 1];
        const QPointF gap = near.gradFrom - far.gradTo;
        if (std::hypot(gap.x(), gap.y()) > 0.01)
            continue;                // not a shared corner — a run ended mid-segment

        const QPointF arrive = unit(far.gradTo - far.gradFrom);   // into the corner
        const QPointF leave = unit(near.gradTo - near.gradFrom);  // and out of it
        if (QPointF::dotProduct(arrive, leave) > 0.9999)
            continue;                // straight through: flat ends already abut

        const QPointF corner = near.gradFrom;
        const QPointF normal = unit(arrive + leave);
        const qreal extent = width() + height();

        // The arriving run lies behind the bisector, the leaving one ahead of it.
        far.to = corner + arrive * HaloWidth;
        far.clipTo = halfPlane(corner, normal, extent);
        near.from = corner - leave * HaloWidth;
        near.clipFrom = halfPlane(corner, -normal, extent);
    }

    return runs;
}

QRegion BorderGlow::regionFor(const QVector<Run> &runs) const
{
    QRegion region;
    for (const Run &run : runs) {
        const QRectF box = QRectF(run.from, run.to).normalized();
        region += box.adjusted(-Pad, -Pad, Pad, Pad).toAlignedRect();
    }
    return region;
}

void BorderGlow::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;

    // Losing focus fades the light out and then stops the clock: the effect belongs to
    // the window being looked at, and there is no reason to spend a frame on it while
    // the user is somewhere else. tick() stops the timer once the fade has finished.
    if (active && !m_suspended && isVisible())
        setRunning(true);
}

void BorderGlow::setRunning(bool run)
{
    if (run == m_timer->isActive())
        return;
    if (run) {
        m_clock.restart();
        m_timer->start();
    } else {
        m_timer->stop();
    }
}

void BorderGlow::setSuspended(bool suspended)
{
    if (m_suspended == suspended)
        return;
    m_suspended = suspended;
    setRunning(!suspended && isVisible() && (m_active || m_level > 0.0));
}

void BorderGlow::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    setRunning(!m_suspended);
}

void BorderGlow::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    setRunning(false);
}

void BorderGlow::tick()
{
    if (m_length <= 0.0)
        return;

    // Clamped so a stalled frame slows the light down rather than teleporting it past
    // the region we are about to repaint, which would leave the old tail on screen.
    const qreal dt = qBound(0.0, m_clock.restart() / 1000.0, 0.06);

    m_step = Speed * dt;
    m_distance = wrap(m_distance + m_step, m_length);
    m_phase = wrap(m_phase + dt / BreathSeconds, 1.0);

    const qreal target = m_active ? 1.0 : 0.0;
    const qreal step = dt / FadeSeconds;
    m_level = target > m_level ? qMin(target, m_level + step) : qMax(target, m_level - step);

    // A tail this long covers most of what it will cover next frame too, so the repaint
    // is the tail stretched backwards by one step: where it is now, plus where it was.
    update(regionFor(buildRuns(tailLength() + m_step)) & m_band);

    // Faded out: the repaint just queued clears the last of it, and there is nothing
    // left to animate until the window is in front of the user again.
    if (!m_active && m_level <= 0.0)
        setRunning(false);
}

void BorderGlow::paintEvent(QPaintEvent *e)
{
    if (m_length <= 0.0 || m_level <= 0.01)
        return;

    const QVector<Run> runs = buildRuns(tailLength());
    if (runs.isEmpty())
        return;

    // Every other widget's repaint reaches us too, because we sit on top of all of them.
    // Nothing outside the tail is ours to draw, so leave without touching the painter.
    if (!e->region().intersects(regionFor(runs)))
        return;

    const bool light = Theme::isLightFamily(Theme::appearance());
    const qreal breath = 0.82 + 0.18 * qSin(m_phase * 2.0 * M_PI);
    // A glow over a white surface reads far heavier than the same one over near-black.
    const qreal strength = m_level * breath * (light ? 0.72 : 1.0);
    const QColor tint = light ? Theme::accentInk() : Theme::accent();
    const qreal tail = tailLength();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Stops are placed along the run rather than at its ends only: the fade is a curve,
    // and a two-stop gradient over a whole edge would straighten it out.
    constexpr int Stops = 12;
    qreal fade[Stops];

    for (const Run &run : runs) {
        for (int i = 0; i < Stops; ++i) {
            const qreal t = qreal(i) / (Stops - 1);
            fade[i] = ramp(run.dTo + (run.dFrom - run.dTo) * t, tail);
        }

        const bool clipped = !run.clipFrom.isEmpty() || !run.clipTo.isEmpty();
        if (clipped) {
            p.save();
            if (!run.clipFrom.isEmpty())
                p.setClipPath(run.clipFrom, Qt::IntersectClip);
            if (!run.clipTo.isEmpty())
                p.setClipPath(run.clipTo, Qt::IntersectClip);
        }

        const auto stroke = [&](qreal penWidth, qreal alpha) {
            QLinearGradient gradient(run.gradTo, run.gradFrom);
            for (int i = 0; i < Stops; ++i) {
                QColor c = tint;
                c.setAlpha(qBound(0, qRound(fade[i] * alpha * 255.0), 255));
                gradient.setColorAt(qreal(i) / (Stops - 1), c);
            }
            p.setPen(QPen(QBrush(gradient), penWidth, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(run.from, run.to);
        };

        // Widest first, so the lit line lands on top of its own haze.
        for (int band = HaloBands; band >= 1; --band)
            stroke(HaloWidth * band / HaloBands, HaloAlpha * strength);
        stroke(CoreWidth, CoreAlpha * strength);

        if (clipped)
            p.restore();
    }
}
