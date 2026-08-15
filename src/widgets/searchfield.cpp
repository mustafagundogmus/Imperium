#include "searchfield.h"
#include "../css.h"
#include "../icons.h"
#include "../theme.h"

#include <QEvent>
#include <QFontMetricsF>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

namespace {

constexpr qreal PadX = 8.0;
constexpr qreal Gap = 7.0;
constexpr int IconSize = 11;
constexpr qreal BadgePadX = 4.0;
constexpr qreal BadgePadY = 1.0;

const QChar ControlGlyph(0x2303);   // ⌃ UP ARROWHEAD

/// IBM Plex Mono does not universally carry U+2303. When it is missing the chevron is
/// drawn by hand instead of letting Qt substitute a glyph from an unrelated family.
bool haveControlGlyph()
{
    static const bool ok = QFontMetricsF(Theme::Font::kbd()).inFont(ControlGlyph);
    return ok;
}

constexpr qreal ChevronW = 5.0;
constexpr qreal ChevronGap = 2.0;

QString badgeText()
{
    return haveControlGlyph() ? QStringLiteral("⌃K") : QStringLiteral("K");
}

qreal drawnChevronWidth()
{
    return haveControlGlyph() ? 0.0 : ChevronW + ChevronGap;
}

} // namespace

SearchField::SearchField(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::Metric::SearchHeight);
    setCursor(Qt::IBeamCursor);

    m_edit = new QLineEdit(this);
    m_edit->setFrame(false);
    m_edit->setFont(Theme::Font::searchText());
    m_edit->setPlaceholderText(QStringLiteral("Tweak ara…"));
    m_edit->setAttribute(Qt::WA_MacShowFocusRect, false);
    // Don't let the field claim focus just for being the first focusable widget — the
    // window should open with the placeholder showing, not a blinking caret. ⌃K and a
    // click both still focus it.
    m_edit->setFocusPolicy(Qt::ClickFocus);
    applyStyle();
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, &SearchField::applyStyle);
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, &SearchField::applyStyle);

    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::textChanged, this, &SearchField::textChanged);

    layoutEditor();
}

void SearchField::applyStyle()
{
    m_edit->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; "
                                         "padding: 0; margin: 0; selection-background-color: %1; "
                                         "selection-color: %2; }")
                              .arg(Theme::accentSoft().name(QColor::HexArgb),
                                   Theme::Color::TextPrimary().name()));

    QPalette pal = m_edit->palette();
    pal.setColor(QPalette::Text, Theme::Color::TextPrimary());
    pal.setColor(QPalette::PlaceholderText, Theme::Color::Placeholder());
    m_edit->setPalette(pal);
    update();
}

QSize SearchField::sizeHint() const
{
    return {0, Theme::Metric::SearchHeight};
}

QString SearchField::text() const
{
    return m_edit->text();
}

void SearchField::clearText()
{
    m_edit->clear();
}

void SearchField::focusField()
{
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
}

qreal SearchField::badgeWidth() const
{
    return 2 * BadgePadX + 2 /*border*/ + drawnChevronWidth()
           + Css::textWidth(Theme::Font::kbd(), badgeText());
}

void SearchField::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutEditor();
}

void SearchField::layoutEditor()
{
    const int x = qRound(PadX + IconSize + Gap);
    const int right = qRound(PadX + badgeWidth() + Gap);
    m_edit->setGeometry(x, 0, qMax(0, width() - x - right), height());
}

void SearchField::mousePressEvent(QMouseEvent *e)
{
    // Clicking the magnifier or the badge should land in the field, not fall through to
    // the window's resize hit-testing.
    if (e->button() == Qt::LeftButton) {
        m_edit->setFocus(Qt::MouseFocusReason);
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

bool SearchField::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
            m_focused = (event->type() == QEvent::FocusIn);
            update();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SearchField::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    p.setPen(QPen(m_focused ? Color::ToggleOffBorder() : Color::BorderControl(), 1.0));
    p.setBrush(Color::Surface());
    p.drawRoundedRect(frame, Metric::ControlRadius, Metric::ControlRadius);

    const qreal dpr = devicePixelRatioF();
    const QPixmap glass = Icons::search(Color::Placeholder(), dpr);
    p.drawPixmap(QPointF(PadX, std::round((height() - IconSize) / 2.0)), glass);

    // ⌃K badge
    const QFont &kbdFont = Font::kbd();
    const qreal bw = badgeWidth();
    const qreal bh = 2 * BadgePadY + 2 + Css::normalLine(kbdFont);
    const QRectF badge(width() - PadX - bw + 0.5,
                       std::round((height() - bh) / 2.0) + 0.5,
                       bw - 1.0, bh - 1.0);

    p.setPen(QPen(Color::BorderControl(), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(badge, Metric::BadgeRadius, Metric::BadgeRadius);

    const QRectF badgeText_(badge.left() + BadgePadX, badge.top(),
                            badge.width() - 2 * BadgePadX, badge.height());
    Css::drawCentered(&p, badgeText_, kbdFont, Color::TextFaint(), badgeText(), Qt::AlignRight);

    if (!haveControlGlyph()) {
        const qreal cx = badgeText_.left() + ChevronW / 2.0;
        const qreal cy = badgeText_.center().y();
        p.setPen(QPen(Color::TextFaint(), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(cx - 2.5, cy + 1.0),
                                  QPointF(cx, cy - 2.0),
                                  QPointF(cx + 2.5, cy + 1.0)}));
    }
}
