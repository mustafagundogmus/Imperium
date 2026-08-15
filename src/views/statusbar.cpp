#include "statusbar.h"
#include "../css.h"
#include "../theme.h"
#include "../widgets/buttons.h"

#include <QPainter>
#include <QTimer>

namespace {
constexpr qreal PadLeft = 18.0;
constexpr qreal PadRight = 12.0;
constexpr qreal Gap = 10.0;
} // namespace

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::Metric::StatusBarHeight);

    m_revert = new PillButton(PillButton::Ghost, QStringLiteral("Geri al"), this);
    m_apply = new PillButton(PillButton::Accent, QStringLiteral("Uygula (0)"), this);

    connect(m_revert, &PillButton::clicked, this, &StatusBar::revertRequested);
    connect(m_apply, &PillButton::clicked, this, &StatusBar::applyRequested);

    m_noticeTimer = new QTimer(this);
    m_noticeTimer->setSingleShot(true);
    m_noticeTimer->setInterval(6000);
    connect(m_noticeTimer, &QTimer::timeout, this, [this] {
        m_notice.clear();
        update();
    });

    setPending(0);
}

QSize StatusBar::sizeHint() const
{
    return {0, Theme::Metric::StatusBarHeight};
}

void StatusBar::setSummary(const QString &summary)
{
    if (m_summary == summary)
        return;
    m_summary = summary;
    update();
}

void StatusBar::setNotice(const QString &text)
{
    m_notice = text;
    m_noticeTimer->start();
    update();
}

void StatusBar::setPending(int count)
{
    m_pendingText = QStringLiteral("%1 değişiklik bekliyor").arg(count);
    m_apply->setText(QStringLiteral("Uygula (%1)").arg(count));

    const bool live = count > 0;
    m_revert->setEnabledLook(live);
    m_apply->setEnabledLook(live);

    // The apply pill's width tracks the count, so the whole row has to re-lay out.
    resizeEvent(nullptr);
    update();
}

void StatusBar::resizeEvent(QResizeEvent *e)
{
    if (e)
        QWidget::resizeEvent(e);

    const int top = qRound((height() - 1 /*top border*/ - m_apply->height()) / 2.0) + 1;

    int x = qRound(width() - PadRight - m_apply->width());
    m_apply->move(x, top);

    x -= qRound(Gap) + m_revert->width();
    m_revert->move(x, qRound((height() - 1 - m_revert->height()) / 2.0) + 1);
}

void StatusBar::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    Css::hairline(&p, QRectF(0, 0, width(), 1), Color::Divider());
    const QRectF content(0, 1, width(), height() - 1);

    const qreal pendingW = Css::textWidth(Font::statusPending(), m_pendingText);
    const qreal pendingX = m_revert->x() - Gap - pendingW;

    Css::drawCentered(&p, QRectF(pendingX, content.top(), pendingW, content.height()),
                      Font::statusPending(), Color::TextStatus(), m_pendingText);

    const QRectF summaryBox(PadLeft, content.top(),
                            qMax(0.0, pendingX - Gap - PadLeft), content.height());
    // A notice takes the summary's place for a few seconds, in the brighter text colour
    // so it reads as something that just happened rather than as static chrome.
    const bool showNotice = !m_notice.isEmpty();
    Css::drawCentered(&p, summaryBox, Font::monoMeta(),
                      showNotice ? Color::TextMono() : Color::TextFaint(),
                      showNotice ? m_notice : m_summary, Qt::AlignLeft, true);
}
