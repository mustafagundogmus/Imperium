#include "applyoverlay.h"

#include "buttons.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../css.h"
#include "../i18n.h"
#include "../shell.h"
#include "../theme.h"

#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVariantAnimation>

namespace {

constexpr qreal CardWidth = 460.0;
constexpr qreal Pad = 16.0;
constexpr qreal RailHeight = 4.0;
constexpr qreal GapAboveRail = 12.0;
constexpr qreal GapBelowRail = 12.0;
constexpr qreal GapNameToPath = 2.0;
constexpr qreal GapToButtons = 14.0;
constexpr int ButtonGap = 8;

// Pacing. The writes themselves take under a millisecond each, so the run is spaced
// out deliberately: fast enough not to feel like waiting, slow enough that the name of
// each tweak can actually be read as it goes past.
constexpr int TargetDurationMs = 3600;
constexpr int MinStepMs = 75;
constexpr int MaxStepMs = 300;

} // namespace

ApplyOverlay::ApplyOverlay(AppState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    hide();
    setFocusPolicy(Qt::StrongFocus);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ApplyOverlay::step);

    m_fade = new QVariantAnimation(this);
    m_fade->setDuration(160);
    m_fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_opacity = v.toReal();
        update();
        if (m_opacity <= 0.001 && m_fade->endValue().toReal() == 0.0)
            hide();
    });

    // The rail chases the real count instead of jumping, so it reads as motion.
    m_progress = new QVariantAnimation(this);
    m_progress->setDuration(320);
    m_progress->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_progress, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_shown = v.toReal();
        update();
    });

    m_shimmer = new QVariantAnimation(this);
    m_shimmer->setDuration(1400);
    m_shimmer->setStartValue(0.0);
    m_shimmer->setEndValue(1.0);
    m_shimmer->setLoopCount(-1);
    connect(m_shimmer, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_shimmerPos = v.toReal();
        if (m_running)
            update();
    });

    m_restartExplorer = new PillButton(PillButton::Accent,
                                       Locale::tr(QStringLiteral("apply.restartExplorer")), this);
    m_close = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("apply.close")), this);
    m_restartExplorer->hide();
    m_close->hide();
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        m_restartExplorer->setText(Locale::tr(QStringLiteral("apply.restartExplorer")));
        m_close->setText(Locale::tr(QStringLiteral("apply.close")));
    });

    connect(m_restartExplorer, &PillButton::clicked, this, [this] {
        QString error;
        if (Shell::restartExplorer(&error))
            Q_EMIT notice(Locale::tr(QStringLiteral("apply.notice.explorerRestarted")));
        else
            Q_EMIT notice(Locale::tr(QStringLiteral("apply.notice.explorerFailed")).arg(error));
        m_fade->setStartValue(m_opacity);
        m_fade->setEndValue(0.0);
        m_fade->start();
    });
    connect(m_close, &PillButton::clicked, this, [this] {
        m_fade->setStartValue(m_opacity);
        m_fade->setEndValue(0.0);
        m_fade->start();
    });

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

void ApplyOverlay::run()
{
    if (m_running)
        return;

    m_queue = m_state->pendingIds().toVector();
    if (m_queue.isEmpty())
        return;
    start();
}

void ApplyOverlay::start()
{
    m_total = int(m_queue.size());
    m_done = m_succeeded = m_failed = 0;
    m_firstError.clear();
    m_elevationRequired = false;
    m_needsExplorer = false;
    m_complete = false;
    m_running = true;
    m_shown = 0.0;
    m_currentName.clear();
    m_currentPath.clear();
    m_summary.clear();

    m_restartExplorer->hide();
    m_close->hide();

    raise();
    show();
    setFocus();

    m_fade->stop();
    m_fade->setStartValue(m_opacity);
    m_fade->setEndValue(1.0);
    m_fade->start();
    m_shimmer->start();

    m_timer->start(qBound(MinStepMs, TargetDurationMs / m_total, MaxStepMs));
}

void ApplyOverlay::step()
{
    if (m_queue.isEmpty()) {
        complete();
        return;
    }

    const QString id = m_queue.takeFirst();
    const AppState::StepOutcome outcome = m_state->applyOne(id);

    ++m_done;
    if (outcome.ok) {
        ++m_succeeded;
        if (const Tweak *tweak = Catalog::instance().tweak(id))
            m_needsExplorer = m_needsExplorer || Shell::needsExplorerRestart(*tweak);
    } else {
        ++m_failed;
        m_elevationRequired = m_elevationRequired || outcome.elevationRequired;
        if (m_firstError.isEmpty())
            m_firstError = outcome.error;
    }

    m_currentName = outcome.name;
    m_currentPath = outcome.path;

    m_progress->stop();
    m_progress->setStartValue(m_shown);
    m_progress->setEndValue(qreal(m_done) / m_total);
    m_progress->start();

    update();
}

void ApplyOverlay::complete()
{
    m_timer->stop();
    m_shimmer->stop();
    m_running = false;
    m_complete = true;

    QStringList parts;
    if (m_succeeded > 0)
        parts << Locale::tr(QStringLiteral("apply.applied")).arg(m_succeeded);
    if (m_failed > 0)
        parts << Locale::tr(QStringLiteral("apply.failed")).arg(m_failed);
    if (m_elevationRequired)
        parts << Locale::tr(QStringLiteral("apply.elevationNeeded"));
    m_summary = parts.join(QStringLiteral(" · "));

    m_currentName = m_needsExplorer
                        ? Locale::tr(QStringLiteral("apply.pendingExplorerRestart"))
                        : Locale::tr(QStringLiteral("apply.allActive"));
    m_currentPath.clear();

    m_restartExplorer->setVisible(m_needsExplorer);
    m_close->show();
    layoutButtons();
    update();

    Q_EMIT finished(m_succeeded, m_failed, m_elevationRequired, m_firstError);
}

QRectF ApplyOverlay::cardRect() const
{
    using namespace Theme;

    const qreal labelLine = Css::normalLine(Font::sectionTitle());
    const qreal nameLine = Css::normalLine(Font::tweakName());
    const qreal pathLine = Css::normalLine(Font::monoMeta());

    qreal h = 2 * Pad + labelLine + GapAboveRail + RailHeight + GapBelowRail
              + nameLine + GapNameToPath + pathLine;
    if (m_complete)
        h += GapToButtons + m_close->height();

    const qreal w = qMin(CardWidth, width() - 2 * Pad);
    return {std::round((width() - w) / 2.0), std::round((height() - h) / 2.0), w, std::round(h)};
}

void ApplyOverlay::layoutButtons()
{
    const QRectF card = cardRect();
    const qreal y = card.bottom() - Pad - m_close->height();

    qreal x = card.right() - Pad - m_close->width();
    m_close->move(qRound(x), qRound(y));

    if (m_restartExplorer->isVisible()) {
        x -= ButtonGap + m_restartExplorer->width();
        m_restartExplorer->move(qRound(x), qRound(y));
    }
}

void ApplyOverlay::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutButtons();
}

void ApplyOverlay::mousePressEvent(QMouseEvent *e)
{
    // The scrim swallows clicks so nothing behind it can be touched mid-write.
    e->accept();
}

void ApplyOverlay::keyPressEvent(QKeyEvent *e)
{
    if (m_complete && (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return)) {
        m_fade->setStartValue(m_opacity);
        m_fade->setEndValue(0.0);
        m_fade->start();
        e->accept();
        return;
    }
    e->accept();
}

void ApplyOverlay::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setOpacity(qBound(0.0, m_opacity, 1.0));

    // scrim
    QColor scrim = Color::Window();
    scrim.setAlpha(226);
    p.fillRect(rect(), scrim);

    const QRectF card = cardRect();
    p.setPen(QPen(Color::TileBorder(), 1.0));
    p.setBrush(Color::Tile());
    p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5),
                      Metric::ControlRadius, Metric::ControlRadius);

    const qreal left = card.left() + Pad;
    const qreal right = card.right() - Pad;
    const QRectF inner(left, 0, right - left, height());

    // --- heading -----------------------------------------------------------
    const QFont &labelFont = Font::sectionTitle();
    const qreal labelLine = Css::normalLine(labelFont);
    qreal y = card.top() + Pad;

    Css::drawText(&p, inner, Css::baseline(labelFont, y, labelLine), labelFont,
                  Color::TextDim(),
                  m_complete ? Locale::tr(QStringLiteral("apply.done"))
                            : Locale::tr(QStringLiteral("apply.running")));

    const QFont &countFont = Font::sectionCount();
    Css::drawText(&p, inner, Css::baseline(countFont, y, labelLine), countFont,
                  Color::TextFainter(),
                  QStringLiteral("%1/%2").arg(m_done).arg(m_total), Qt::AlignRight);

    // --- rail --------------------------------------------------------------
    y += labelLine + GapAboveRail;
    const QRectF rail(left, y, inner.width(), RailHeight);
    const qreal radius = RailHeight / 2.0;

    QPainterPath railPath;
    railPath.addRoundedRect(rail, radius, radius);
    p.setPen(Qt::NoPen);
    p.setBrush(Color::ToggleOff());
    p.drawPath(railPath);

    const qreal filled = qBound(0.0, m_shown, 1.0) * rail.width();
    if (filled > 0.5) {
        p.save();
        p.setClipPath(railPath);
        p.setBrush(Theme::accent());
        p.drawRect(QRectF(rail.left(), rail.top(), filled, rail.height()));

        // A highlight travels along the filled part while there is work left, which is
        // what separates "still going" from "stalled" at a glance.
        if (m_running) {
            const qreal band = qMax(60.0, rail.width() * 0.22);
            const qreal x = rail.left() - band + (filled + band) * m_shimmerPos;
            QLinearGradient sweep(x, 0, x + band, 0);
            QColor bright = Color::TextPrimary();
            bright.setAlpha(70);
            sweep.setColorAt(0.0, QColor(255, 255, 255, 0));
            sweep.setColorAt(0.5, bright);
            sweep.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(sweep);
            p.drawRect(QRectF(rail.left(), rail.top(), filled, rail.height()));
        }
        p.restore();
    }

    // --- current item ------------------------------------------------------
    y += RailHeight + GapBelowRail;
    const QFont &nameFont = Font::tweakName();
    const qreal nameLine = Css::normalLine(nameFont);
    Css::drawText(&p, inner, Css::baseline(nameFont, y, nameLine), nameFont,
                  Color::TextPrimary(),
                  m_complete && !m_summary.isEmpty() ? m_summary : m_currentName,
                  Qt::AlignLeft, true);

    y += nameLine + GapNameToPath;
    const QFont &pathFont = Font::monoMeta();
    const qreal pathLine = Css::normalLine(pathFont);
    Css::drawText(&p, inner, Css::baseline(pathFont, y, pathLine), pathFont,
                  Color::TextFaint(),
                  m_complete ? m_currentName : m_currentPath, Qt::AlignLeft, true);
}
