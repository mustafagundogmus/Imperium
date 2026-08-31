#include "smoothscrollarea.h"
#include "../settings.h"
#include "../theme.h"

#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

SmoothScrollArea::SmoothScrollArea(QWidget *parent)
    : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setWidgetResizable(true);
    viewport()->setAutoFillBackground(false);
    applyStyle();
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged,
            this, &SmoothScrollArea::applyStyle);

    m_anim = new QPropertyAnimation(verticalScrollBar(), "value", this);
    m_anim->setDuration(190);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void SmoothScrollArea::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical {
            background: transparent;
            width: %1px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %2;
            border: 2px solid %3;
            border-radius: %4px;
            min-height: 28px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0; width: 0; background: transparent; border: none;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
    )")
                      .arg(Theme::Metric::ScrollBarWidth)
                      .arg(Theme::Color::ScrollThumb().name(), Theme::Color::Window().name())
                      // Half the track, so the handle stays a capsule whatever the track
                      // is widened to. It was a literal 4 next to a literal 8 and the two
                      // were the same number by coincidence, not by construction.
                      .arg(Theme::Metric::ScrollBarWidth / 2));
}

void SmoothScrollArea::scrollToTop()
{
    m_anim->stop();
    verticalScrollBar()->setValue(0);
    m_target = 0;
}

void SmoothScrollArea::wheelEvent(QWheelEvent *e)
{
    QScrollBar *bar = verticalScrollBar();
    if (!Settings::instance().smoothScroll() || !bar->isVisible()
        || bar->maximum() == 0 || e->angleDelta().y() == 0) {
        QScrollArea::wheelEvent(e);
        return;
    }

    // Three "lines" per notch, matching Qt's default step, but eased.
    const int step = bar->singleStep() * 3;
    const int delta = e->angleDelta().y() / 120 * step;

    if (m_anim->state() != QAbstractAnimation::Running)
        m_target = bar->value();
    m_target = qBound(bar->minimum(), m_target - delta, bar->maximum());

    m_anim->stop();
    m_anim->setStartValue(bar->value());
    m_anim->setEndValue(m_target);
    m_anim->start();

    e->accept();
}
