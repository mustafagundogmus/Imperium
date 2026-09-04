#include "fluenttitlebar.h"
#include "../css.h"
#include "../i18n.h"
#include "../icons.h"
#include "../theme.h"

#include <QCoreApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

namespace {
constexpr qreal PadLeft = 14.0;
constexpr qreal Gap = 10.0;
constexpr int LogoSize = 18;
constexpr int ThemeButtonHeight = 28;
constexpr qreal ThemePadX = 10.0;
constexpr qreal ThemeGap = 6.0;
constexpr int ThemeDot = 12;
constexpr qreal ThemeMarginRight = 8.0;
constexpr int GlyphSize = 10;

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

// ------------------------------------------------------------- window button ---

FluentWindowButton::FluentWindowButton(Kind kind, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
{
    setFixedSize(Theme::Fluent::WindowButtonWidth, Theme::Fluent::TitleBarHeight);
    setAttribute(Qt::WA_Hover, true);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

void FluentWindowButton::setKind(Kind kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    update();
}

void FluentWindowButton::setCornerRadius(qreal radius)
{
    m_corner = radius;
    update();
}

void FluentWindowButton::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void FluentWindowButton::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void FluentWindowButton::mousePressEvent(QMouseEvent *e)
{
    // Accepted, or the press reaches the title bar and becomes a system move that never
    // sends its release back here — see WindowButton for the history.
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void FluentWindowButton::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void FluentWindowButton::mouseReleaseEvent(QMouseEvent *e)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (wasPressed && rect().contains(e->pos()))
            Q_EMIT clicked();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void FluentWindowButton::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool close = m_kind == Close;
    if (m_hovered) {
        QPainterPath path;
        const QRectF r = rect();
        if (m_corner > 0) {
            path.moveTo(r.left(), r.top());
            path.lineTo(r.right() - m_corner, r.top());
            path.arcTo(r.right() - 2 * m_corner, r.top(), 2 * m_corner, 2 * m_corner, 90, -90);
            path.lineTo(r.right(), r.bottom());
            path.lineTo(r.left(), r.bottom());
            path.closeSubpath();
        } else {
            path.addRect(r);
        }
        p.fillPath(path, close ? t.closeHover : t.subtleHover);
    }

    const QColor ink = (m_hovered && close) ? QColor(Qt::white) : Theme::Color::IconStroke();
    const qreal dpr = devicePixelRatioF();
    QPixmap g;
    switch (m_kind) {
    case Minimize: g = Icons::windowMinimize(ink, dpr); break;
    case Maximize: g = Icons::windowMaximize(ink, dpr); break;
    case Restore:  g = Icons::windowRestore(ink, dpr); break;
    case Close:    g = Icons::windowClose(ink, dpr); break;
    }
    const QSizeF gs = g.deviceIndependentSize();
    p.drawPixmap(QPointF(std::round((width() - gs.width()) / 2.0),
                         std::round((height() - gs.height()) / 2.0)),
                 g);
}

// -------------------------------------------------------------- theme button ---

FluentThemeButton::FluentThemeButton(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    refreshGeometry();
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, &FluentThemeButton::refreshGeometry);
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, &FluentThemeButton::refreshGeometry);
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &FluentThemeButton::refreshGeometry);
}

QString FluentThemeButton::label() const
{
    return Locale::tr(Theme::isLightFamily(Theme::appearance()) ? QStringLiteral("fluent.theme.light")
                                                                : QStringLiteral("fluent.theme.dark"));
}

QSize FluentThemeButton::sizeHint() const
{
    return {qCeil(2 * ThemePadX + ThemeDot + ThemeGap + Css::textWidth(Theme::sans(12), label())),
            ThemeButtonHeight};
}

void FluentThemeButton::refreshGeometry()
{
    setFixedSize(sizeHint());
    updateGeometry();
    update();
}

void FluentThemeButton::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void FluentThemeButton::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void FluentThemeButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void FluentThemeButton::mouseReleaseEvent(QMouseEvent *e)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (wasPressed && rect().contains(e->pos())) {
            // The user's own scheme is what is stored; the Fluent shell only reads its
            // family. Flipping writes the plain member of the other family, so the
            // classic shell comes back to Dark or Light rather than to a tinted scheme
            // the user never picked.
            Theme::setAppearance(Theme::isLightFamily(Theme::appearance()) ? Theme::Appearance::Dark
                                                                           : Theme::Appearance::Light);
        }
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void FluentThemeButton::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.subtleHover);
        p.drawRoundedRect(rect(), 4.0, 4.0);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(t.accent);
    p.drawEllipse(QRectF(ThemePadX, (height() - ThemeDot) / 2.0, ThemeDot, ThemeDot));
    Css::drawCentered(&p, QRectF(ThemePadX + ThemeDot + ThemeGap, 0, width(), height()),
                      Theme::sans(12), t.textSec, label());
}

// ------------------------------------------------------------------ title bar ---

FluentTitleBar::FluentTitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::Fluent::TitleBarHeight);

    m_theme = new FluentThemeButton(this);
    m_minimize = new FluentWindowButton(FluentWindowButton::Minimize, this);
    m_maximize = new FluentWindowButton(FluentWindowButton::Maximize, this);
    m_close = new FluentWindowButton(FluentWindowButton::Close, this);
    m_close->setCornerRadius(Theme::Fluent::WindowRadius);

    connect(m_minimize, &FluentWindowButton::clicked, this, &FluentTitleBar::minimizeRequested);
    connect(m_maximize, &FluentWindowButton::clicked, this, &FluentTitleBar::maximizeToggleRequested);
    connect(m_close, &FluentWindowButton::clicked, this, &FluentTitleBar::closeRequested);

    // The theme button's width follows its word, which follows the family, the face and
    // the language; it resizes itself on those signals and this places it afterwards —
    // connected after the button's own slots, so it runs second.
    const auto place = [this] { resizeEvent(nullptr); update(); };
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, place);
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, place);
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, place);
}

QSize FluentTitleBar::sizeHint() const
{
    return {0, Theme::Fluent::TitleBarHeight};
}

void FluentTitleBar::setMaximized(bool maximized)
{
    m_maximize->setKind(maximized ? FluentWindowButton::Restore : FluentWindowButton::Maximize);
    // A maximised window has no rounded corner for the close button to respect.
    m_close->setCornerRadius(maximized ? 0.0 : Theme::Fluent::WindowRadius);
}

void FluentTitleBar::resizeEvent(QResizeEvent *e)
{
    if (e)
        QWidget::resizeEvent(e);
    const int w = Theme::Fluent::WindowButtonWidth;
    m_close->move(width() - w, 0);
    m_maximize->move(width() - 2 * w, 0);
    m_minimize->move(width() - 3 * w, 0);
    m_theme->move(qRound(width() - 3 * w - ThemeMarginRight - m_theme->width()),
                  qRound((height() - m_theme->height()) / 2.0));
}

void FluentTitleBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        if (QWindow *handle = window()->windowHandle()) {
            handle->startSystemMove();
            e->accept();
            return;
        }
    }
    QWidget::mousePressEvent(e);
}

void FluentTitleBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        Q_EMIT maximizeToggleRequested();
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void FluentTitleBar::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // The logo, the name and the version as one block, centred on the window — not on
    // this bar, which since the rail and the pane took the left end of the top band is
    // only the content column wide, so centring on it put the block right of the window's
    // middle. The card is the parent; its middle, in this bar's coordinates, is where the
    // block's middle goes, held clear of the theme button when the window is narrow.
    static const QIcon appIcon(QStringLiteral(":/icons/tweaker.ico"));
    const QPixmap logo = appIcon.pixmap(QSize(LogoSize, LogoSize), devicePixelRatioF());

    const QFont nameFont = Theme::sans(12, Theme::Weight::Medium);
    const QFont versionFont = Theme::sans(11);
    const qreal nameW = Css::textWidth(nameFont, appName());
    const qreal versionW = Css::textWidth(versionFont, appVersion());
    const qreal blockW = LogoSize + Gap + nameW + (appVersion().isEmpty() ? 0.0 : Gap + versionW);

    const qreal windowW = parentWidget() ? parentWidget()->width() : width();
    const qreal middle = windowW / 2.0 - x();
    qreal x = std::round(middle - blockW / 2.0);
    x = qMin(x, m_theme->x() - Gap - blockW);
    x = qMax(x, PadLeft);
    p.drawPixmap(QPointF(x, std::round((height() - LogoSize) / 2.0)), logo);
    x += LogoSize + Gap;
    Css::drawCentered(&p, QRectF(x, 0, nameW, height()), nameFont, t.text, appName());
    x += nameW + Gap;
    Css::drawCentered(&p, QRectF(x, 0, versionW, height()), versionFont, t.textMuted, appVersion());
}
