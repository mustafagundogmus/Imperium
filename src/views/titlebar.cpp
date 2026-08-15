#include "titlebar.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"
#include "../widgets/buttons.h"

#include <QCoreApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

namespace {
constexpr qreal PadLeft = 12.0;
constexpr qreal Gap = 8.0;
constexpr qreal SummaryRightMargin = 14.0;
// The handoff draws a 10px accent diamond here; the project's own knob icon replaces it
// and needs a couple more pixels to stay legible.
constexpr int LogoSize = 16;

// Name and version come from QCoreApplication so the title bar, the window title and the
// executable's version block can never drift apart.
QString appName()
{
    const QString name = QCoreApplication::applicationName();
    return name.isEmpty() ? QStringLiteral("Arbitrium") : name;
}

QString appVersion()
{
    const QString version = QCoreApplication::applicationVersion();
    return version.isEmpty() ? QString() : QLatin1Char('v') + version;
}
} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::Metric::TitleBarHeight);

    m_minimize = new WindowButton(WindowButton::Minimize, this);
    m_maximize = new WindowButton(WindowButton::Maximize, this);
    m_close = new WindowButton(WindowButton::Close, this);

    connect(m_minimize, &WindowButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximize, &WindowButton::clicked, this, &TitleBar::maximizeToggleRequested);
    connect(m_close, &WindowButton::clicked, this, &TitleBar::closeRequested);

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
}

QSize TitleBar::sizeHint() const
{
    return {0, Theme::Metric::TitleBarHeight};
}

void TitleBar::setSystemSummary(const QString &summary)
{
    if (m_summary == summary)
        return;
    m_summary = summary;
    update();
}

void TitleBar::setMaximized(bool maximized)
{
    m_maximize->setKind(maximized ? WindowButton::Restore : WindowButton::Maximize);
}

void TitleBar::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    const int w = Theme::Metric::WindowButtonWidth;
    m_close->move(width() - w, 0);
    m_maximize->move(width() - 2 * w, 0);
    m_minimize->move(width() - 3 * w, 0);
}

void TitleBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    if (QWindow *handle = window()->windowHandle()) {
        handle->startSystemMove();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        Q_EMIT maximizeToggleRequested();
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void TitleBar::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal dpr = devicePixelRatioF();

    // The 36px box includes its own 1px bottom border (`box-sizing:border-box`).
    Css::hairline(&p, QRectF(0, height() - 1, width(), 1), Color::Divider());
    const QRectF content(0, 0, width(), height() - 1);

    static const QIcon appIcon(QStringLiteral(":/icons/tweaker.ico"));
    const QPixmap logo = appIcon.pixmap(QSize(LogoSize, LogoSize), dpr);
    p.drawPixmap(QPointF(PadLeft, std::round((content.height() - LogoSize) / 2.0)), logo);

    const QFont &nameFont = Font::appName();
    const QFont &metaFont = Font::monoMeta();

    const QString name = appName();
    const QString version = appVersion();

    qreal x = PadLeft + LogoSize + Gap;
    const qreal nameW = Css::textWidth(nameFont, name);
    Css::drawCentered(&p, QRectF(x, 0, nameW, content.height()), nameFont,
                      Color::TextPrimary(), name);

    x += nameW + Gap;
    const qreal versionW = Css::textWidth(metaFont, version);
    Css::drawCentered(&p, QRectF(x, 0, versionW, content.height()), metaFont,
                      Color::TextFaint(), version);

    if (!m_summary.isEmpty()) {
        const qreal right = 3.0 * Metric::WindowButtonWidth + SummaryRightMargin;
        const qreal left = x + versionW + Gap;
        const QRectF box(left, 0, qMax(0.0, width() - right - left), content.height());
        Css::drawCentered(&p, box, metaFont, Color::TextFaint(), m_summary, Qt::AlignRight, true);
    }
}
