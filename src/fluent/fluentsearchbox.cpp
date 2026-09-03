#include "fluentsearchbox.h"
#include "../css.h"
#include "../i18n.h"
#include "../icons.h"
#include "../theme.h"

#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {
constexpr int Height = 32;
constexpr qreal PadX = 10.0;
constexpr qreal Gap = 8.0;
constexpr int IconSize = 14;
constexpr qreal Radius = 4.0;
constexpr qreal BadgePadX = 4.0;
// The handoff's 14px magnifier: a circle and a handle in lucide's 24 grid.
const QString SearchPath = QStringLiteral("M11 4a7 7 0 1 0 0 14a7 7 0 1 0 0-14M20 20l-3.5-3.5");
const QString Badge = QStringLiteral("Ctrl+K");
} // namespace

FluentSearchBox::FluentSearchBox(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::IBeamCursor);
    setFixedHeight(Height);

    m_edit = new QLineEdit(this);
    m_edit->setFrame(false);
    m_edit->setFont(Theme::sans(13));
    m_edit->setPlaceholderText(Locale::tr(QStringLiteral("fluent.search.placeholder")));
    m_edit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_edit->setFocusPolicy(Qt::ClickFocus);
    m_edit->installEventFilter(this);
    applyStyle();

    connect(m_edit, &QLineEdit::textChanged, this, &FluentSearchBox::textChanged);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, &FluentSearchBox::applyStyle);
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        m_edit->setFont(Theme::sans(13));
        layoutEditor();
        update();
    });
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        m_edit->setPlaceholderText(Locale::tr(QStringLiteral("fluent.search.placeholder")));
    });
    layoutEditor();
}

void FluentSearchBox::applyStyle()
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();
    m_edit->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; "
                                         "padding: 0; margin: 0; selection-background-color: %1; "
                                         "selection-color: %2; }")
                              .arg(t.accentSoft.name(QColor::HexArgb), t.text.name()));
    QPalette pal = m_edit->palette();
    pal.setColor(QPalette::Text, t.text);
    pal.setColor(QPalette::PlaceholderText, Theme::Color::Placeholder());
    m_edit->setPalette(pal);
    update();
}

QSize FluentSearchBox::sizeHint() const
{
    return {0, Height};
}

QString FluentSearchBox::text() const
{
    return m_edit->text();
}

void FluentSearchBox::setText(const QString &text)
{
    m_edit->setText(text);
}

void FluentSearchBox::clearText()
{
    m_edit->clear();
}

void FluentSearchBox::focusField()
{
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
}

qreal FluentSearchBox::badgeWidth() const
{
    return 2 * BadgePadX + 2 + Css::textWidth(Theme::mono(10), Badge);
}

void FluentSearchBox::layoutEditor()
{
    const int x = qRound(PadX + IconSize + Gap);
    const int right = qRound(PadX + badgeWidth() + Gap);
    m_edit->setGeometry(x, 1, qMax(0, width() - x - right), height() - 2);
}

void FluentSearchBox::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutEditor();
}

void FluentSearchBox::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_edit->setFocus(Qt::MouseFocusReason);
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

bool FluentSearchBox::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        m_focused = event->type() == QEvent::FocusIn;
        update();
    }
    return QWidget::eventFilter(watched, event);
}

void FluentSearchBox::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    p.setPen(QPen(t.controlBorder, 1.0));
    p.setBrush(t.controlBg);
    p.drawRoundedRect(frame, Radius, Radius);

    // The underline: focused, it is the accent and two pixels thick, the way a Fluent
    // TextBox answers focus; otherwise textMuted, one pixel.
    if (m_focused) {
        Css::hairline(&p, QRectF(Radius, height() - 2, width() - 2 * Radius, 2), t.accent);
    } else {
        Css::hairline(&p, QRectF(Radius, height() - 1, width() - 2 * Radius, 1), t.textMuted);
    }

    const QPixmap glass = Icons::strokePath(SearchPath, 24.0, QSize(IconSize, IconSize), t.textSec,
                                            2.0, devicePixelRatioF());
    p.drawPixmap(QPointF(PadX, std::round((height() - IconSize) / 2.0)), glass);

    const QFont badgeFont = Theme::mono(10);
    const qreal bw = badgeWidth();
    const qreal bh = Css::normalLine(badgeFont) + 2.0;
    const QRectF badge(width() - PadX - bw + 0.5, std::round((height() - bh) / 2.0) + 0.5,
                       bw - 1.0, bh - 1.0);
    p.setPen(QPen(t.controlBorder, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(badge, 3.0, 3.0);
    Css::drawCentered(&p, badge.adjusted(BadgePadX, 0, -BadgePadX, 0), badgeFont, t.textMuted, Badge,
                      Qt::AlignHCenter);
}
