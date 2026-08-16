#include "splashscreen.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QtMath>

namespace {

constexpr int CardW = 380;
constexpr int CardH = 190;
constexpr int Margin = 24;      // room for the shadow the card paints itself
constexpr int FrameMs = 16;

constexpr qreal RunMs = 2400.0;   // the sequence, start to finish
constexpr qreal FadeMs = 340.0;

/// Every stage is a window into the run, eased so nothing starts or stops abruptly.
qreal stage(qreal t, qreal from, qreal to)
{
    if (t <= from)
        return 0.0;
    if (t >= to)
        return 1.0;
    const qreal u = (t - from) / (to - from);
    return u * u * (3.0 - 2.0 * u);   // smoothstep
}

/// The mark: a diamond, drawn as a path so it can be stroked on progressively.
QPainterPath diamond(const QPointF &centre, qreal radius)
{
    QPainterPath path;
    path.moveTo(centre.x(), centre.y() - radius);
    path.lineTo(centre.x() + radius, centre.y());
    path.lineTo(centre.x(), centre.y() + radius);
    path.lineTo(centre.x() - radius, centre.y());
    path.closeSubpath();
    return path;
}

} // namespace

SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFixedSize(CardW + 2 * Margin, CardH + 2 * Margin);

    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        move(available.center() - QPoint(width() / 2, height() / 2));
    }

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(FrameMs);
    connect(m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));

    m_clock.start();
    m_timer->start();
}

qreal SplashScreen::progress() const
{
    return qBound(0.0, m_clock.elapsed() / RunMs, 1.0);
}

void SplashScreen::finish(QWidget *window)
{
    // Let the sequence play out if the loading beat it, but never hold the user up for
    // longer than the animation itself was going to take.
    while (progress() < 1.0) {
        QApplication::processEvents(QEventLoop::AllEvents, 16);
        QThread::msleep(4);
    }

    QElapsedTimer fade;
    fade.start();
    while (fade.elapsed() < FadeMs) {
        m_fade = 1.0 - qBound(0.0, fade.elapsed() / FadeMs, 1.0);
        update();
        QApplication::processEvents(QEventLoop::AllEvents, 8);
        QThread::msleep(4);
    }

    m_timer->stop();
    if (window)
        window->show();
    close();
}

void SplashScreen::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    const qreal t = progress();
    const QRectF card(Margin, Margin, CardW, CardH);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setOpacity(m_fade);

    // --- the card ----------------------------------------------------------
    // It arrives by growing the last couple of pixels rather than by sliding: the window
    // it turns into does not slide either.
    const qreal appear = stage(t, 0.0, 0.22);
    const qreal grow = 0.985 + 0.015 * appear;
    p.save();
    p.translate(card.center());
    p.scale(grow, grow);
    p.translate(-card.center());

    p.setOpacity(m_fade * appear);
    p.setPen(QPen(Color::BorderWindow(), 1.0));
    p.setBrush(Color::Window());
    p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

    // --- the mark ----------------------------------------------------------
    const QPointF markCentre(card.center().x(), card.top() + 62);
    const qreal draw = stage(t, 0.10, 0.62);

    QPainterPath mark = diamond(markCentre, 21);
    if (draw > 0.0) {
        // Stroked on a percentage at a time, so the shape writes itself.
        QPainterPath partial;
        const int steps = 64;
        const int upto = qMax(1, int(steps * draw));
        partial.moveTo(mark.pointAtPercent(0.0));
        for (int i = 1; i <= upto; ++i)
            partial.lineTo(mark.pointAtPercent(qreal(i) / steps));

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(accent(), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(partial);
    }

    // The inner fill lands once the outline has closed.
    const qreal fill = stage(t, 0.58, 0.80);
    if (fill > 0.0) {
        QColor inner = accent();
        inner.setAlphaF(0.16 * fill);
        p.setPen(Qt::NoPen);
        p.setBrush(inner);
        p.drawPath(diamond(markCentre, 21 * (0.72 + 0.28 * fill)));
    }

    // --- the wordmark ------------------------------------------------------
    const qreal word = stage(t, 0.34, 0.86);
    if (word > 0.0) {
        // Letter-spacing eases from wide to the design's own value: the name settles.
        QFont font = mono(15, Weight::Medium, 0.02 + 0.22 * (1.0 - word));
        const qreal line = Css::normalLine(font);
        const QRectF box(card.left(), card.top() + 104, card.width(), line);

        QColor ink = Color::TextPrimary();
        ink.setAlphaF(word);
        p.setOpacity(m_fade * appear);
        Css::drawText(&p, box, Css::baseline(font, box.top(), line), font, ink,
                      Locale::tr(QStringLiteral("splash.brand")), Qt::AlignHCenter);
    }

    // --- the rule ----------------------------------------------------------
    // The same light that walks the window's border, once, under the name.
    const qreal sweep = stage(t, 0.52, 1.0);
    if (sweep > 0.0) {
        const qreal y = card.top() + 134;
        const qreal full = card.width() - 2 * 56;
        const qreal x = card.left() + 56;

        Css::hairline(&p, QRectF(x, y, full, 1.0), Color::Divider());

        QLinearGradient light(x, 0, x + full, 0);
        QColor head = accent();
        QColor tail = accent();
        tail.setAlpha(0);
        const qreal pos = qBound(0.0, sweep, 1.0);
        light.setColorAt(qMax(0.0, pos - 0.22), tail);
        light.setColorAt(pos, head);
        light.setColorAt(qMin(1.0, pos + 0.02), tail);
        p.fillRect(QRectF(x, y, full, 1.0), light);
    }

    // --- the line under it -------------------------------------------------
    const qreal note = stage(t, 0.66, 1.0);
    if (note > 0.0) {
        const QFont &font = Font::monoMeta();
        const qreal line = Css::normalLine(font);
        QColor ink = Color::TextFaint();
        ink.setAlphaF(note);
        Css::drawText(&p, QRectF(card.left(), card.top() + 150, card.width(), line),
                      Css::baseline(font, card.top() + 150, line), font, ink,
                      Locale::tr(QStringLiteral("splash.tagline")), Qt::AlignHCenter);
    }

    p.restore();
}
