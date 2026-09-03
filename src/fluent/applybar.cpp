#include "applybar.h"
#include "fluentbutton.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>

namespace {
constexpr qreal PadX = Theme::Fluent::ContentPadX;
constexpr qreal Gap = 12.0;
constexpr qreal Dot = 8.0;
const QString CheckPath = QStringLiteral("m5 12 5 5L20 7");
} // namespace

ApplyBar::ApplyBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::Fluent::ApplyBarHeight);

    m_journal = new FluentButton(FluentButton::Secondary, Locale::tr(QStringLiteral("fluent.apply.journal")), this);
    m_revert = new FluentButton(FluentButton::Secondary, Locale::tr(QStringLiteral("fluent.apply.discard")), this);
    m_apply = new FluentButton(FluentButton::Primary, Locale::tr(QStringLiteral("fluent.apply.apply")), this);
    m_apply->setLeadingIcon(CheckPath, 14, 2.25);

    connect(m_journal, &FluentButton::clicked, this, &ApplyBar::journalRequested);
    connect(m_revert, &FluentButton::clicked, this, &ApplyBar::revertRequested);
    connect(m_apply, &FluentButton::clicked, this, &ApplyBar::applyRequested);

    m_noticeTimer = new QTimer(this);
    m_noticeTimer->setSingleShot(true);
    m_noticeTimer->setInterval(6000);
    connect(m_noticeTimer, &QTimer::timeout, this, [this] {
        m_notice.clear();
        update();
    });

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        m_journal->setText(Locale::tr(QStringLiteral("fluent.apply.journal")));
        m_revert->setText(Locale::tr(QStringLiteral("fluent.apply.discard")));
        m_apply->setText(Locale::tr(QStringLiteral("fluent.apply.apply")));
        resizeEvent(nullptr);
        update();
    });
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] { resizeEvent(nullptr); });
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));

    setPending(0);
}

QSize ApplyBar::sizeHint() const
{
    return {0, Theme::Fluent::ApplyBarHeight};
}

void ApplyBar::setPending(int count)
{
    m_pending = count;
    m_revert->setEnabledLook(count > 0);
    m_apply->setEnabledLook(count > 0);
    update();
}

void ApplyBar::setNotice(const QString &text)
{
    m_notice = text;
    m_noticeTimer->start();
    update();
}

void ApplyBar::setCornerRadius(qreal radius)
{
    m_corner = radius;
    update();
}

void ApplyBar::resizeEvent(QResizeEvent *e)
{
    if (e)
        QWidget::resizeEvent(e);
    const auto place = [this](FluentButton *b, int right) {
        b->move(right - b->width(), qRound((height() - b->height()) / 2.0));
        return b->x() - qRound(Gap);
    };
    int right = qRound(width() - PadX);
    right = place(m_apply, right);
    right = place(m_revert, right);
    place(m_journal, right);
}

void ApplyBar::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Mica, with the window's bottom-right corner rounded when it is ours to round.
    QPainterPath path;
    const QRectF r = rect();
    if (m_corner > 0) {
        path.moveTo(r.left(), r.top());
        path.lineTo(r.right(), r.top());
        path.lineTo(r.right(), r.bottom() - m_corner);
        path.arcTo(r.right() - 2 * m_corner, r.bottom() - 2 * m_corner, 2 * m_corner, 2 * m_corner, 0, -90);
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
    } else {
        path.addRect(r);
    }
    p.fillPath(path, t.mica);
    Css::hairline(&p, QRectF(0, 0, width(), 1), t.cardBorder);

    p.setPen(Qt::NoPen);
    p.setBrush(m_pending > 0 ? t.accent : t.textMuted);
    p.drawEllipse(QRectF(PadX, (height() - Dot) / 2.0, Dot, Dot));

    const QString text = !m_notice.isEmpty()
                             ? m_notice
                             : (m_pending > 0
                                    ? Locale::tr(QStringLiteral("fluent.apply.pending")).arg(m_pending)
                                    : Locale::tr(QStringLiteral("fluent.apply.clean")));
    const qreal textX = PadX + Dot + Gap;
    Css::drawCentered(&p, QRectF(textX, 0, qMax(0.0, m_journal->x() - Gap - textX), height()),
                      Theme::sans(13), m_notice.isEmpty() ? t.textSec : t.text, text, Qt::AlignLeft, true);
}
