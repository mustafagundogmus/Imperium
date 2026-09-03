#include "fluentheader.h"
#include "fluentbutton.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {
constexpr qreal PadTop = 28.0;
constexpr qreal PadX = Theme::Fluent::ContentPadX;
constexpr qreal BlockGap = 16.0;
constexpr qreal TextGap = 6.0;
constexpr qreal CrumbGap = 6.0;
constexpr qreal GroupGap = 24.0;

constexpr qreal SegPad = 3.0;
constexpr qreal SegGap = 2.0;
constexpr qreal SegH = 26.0;
constexpr qreal SegPadX = 12.0;
constexpr qreal SegRadius = 3.0;
constexpr qreal SegCountGap = 6.0;
constexpr qreal OuterRadius = 5.0;

const QString SlidersPath = QStringLiteral(
    "M20 7h-9M14 17H5M17 14a3 3 0 1 0 0 6a3 3 0 1 0 0-6M7 4a3 3 0 1 0 0 6a3 3 0 1 0 0-6");
} // namespace

// ------------------------------------------------------------- segmented ---

FluentSegmented::FluentSegmented(const QStringList &labels, QWidget *parent)
    : QWidget(parent)
    , m_labels(labels)
    , m_counts(labels.size(), 0)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    refreshGeometry();
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, &FluentSegmented::refreshGeometry);
}

qreal FluentSegmented::segmentWidth(int index) const
{
    qreal w = 2 * SegPadX + Css::textWidth(Theme::sans(12), m_labels.value(index));
    w += SegCountGap + Css::textWidth(Theme::mono(11), QString::number(m_counts.value(index)));
    return w;
}

QSize FluentSegmented::sizeHint() const
{
    qreal w = 2 + 2 * SegPad;
    for (int i = 0; i < m_labels.size(); ++i)
        w += segmentWidth(i) + (i > 0 ? SegGap : 0.0);
    return {qCeil(w), qRound(2 + 2 * SegPad + SegH)};
}

void FluentSegmented::refreshGeometry()
{
    setFixedSize(sizeHint());
    updateGeometry();
    update();
}

void FluentSegmented::setCurrentIndex(int index)
{
    const int clamped = qBound(0, index, int(m_labels.size()) - 1);
    if (clamped == m_current)
        return;
    m_current = clamped;
    update();
}

void FluentSegmented::setLabels(const QStringList &labels)
{
    if (labels.size() != m_labels.size())
        return;
    m_labels = labels;
    refreshGeometry();
}

void FluentSegmented::setCounts(const QVector<int> &counts)
{
    m_counts = counts;
    m_counts.resize(m_labels.size());
    refreshGeometry();
}

int FluentSegmented::segmentAt(const QPointF &pos) const
{
    qreal x = 1 + SegPad;
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal w = segmentWidth(i);
        if (pos.x() >= x && pos.x() < x + w)
            return i;
        x += w + SegGap;
    }
    return -1;
}

void FluentSegmented::mouseMoveEvent(QMouseEvent *e)
{
    const int hit = segmentAt(e->position());
    if (hit != m_hovered) {
        m_hovered = hit;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void FluentSegmented::leaveEvent(QEvent *e)
{
    m_hovered = -1;
    m_pressed = -1;
    update();
    QWidget::leaveEvent(e);
}

void FluentSegmented::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = segmentAt(e->position());
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void FluentSegmented::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const int hit = segmentAt(e->position());
        const int pressed = m_pressed;
        m_pressed = -1;
        e->accept();
        if (hit >= 0 && hit == pressed && hit != m_current) {
            m_current = hit;
            update();
            Q_EMIT currentIndexChanged(m_current);
        }
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void FluentSegmented::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.setPen(QPen(t.controlBorder, 1.0));
    p.setBrush(t.controlBg);
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), OuterRadius, OuterRadius);

    const QFont labelFont = Theme::sans(12);
    const QFont countFont = Theme::mono(11);
    qreal x = 1 + SegPad;
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal w = segmentWidth(i);
        const QRectF seg(x, 1 + SegPad, w, SegH);
        const bool active = i == m_current;
        if (active) {
            // `0 1px 2px rgba(0,0,0,.15)`: one soft pixel under the card.
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 38));
            p.drawRoundedRect(seg.translated(0, 1), SegRadius, SegRadius);
            p.setBrush(t.card);
            p.drawRoundedRect(seg, SegRadius, SegRadius);
        } else if (i == m_hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.subtleHover);
            p.drawRoundedRect(seg, SegRadius, SegRadius);
        }
        const QColor fg = active ? t.text : t.textSec;
        const QString label = m_labels.at(i);
        const qreal labelW = Css::textWidth(labelFont, label);
        Css::drawCentered(&p, QRectF(x + SegPadX, seg.top(), labelW, SegH), labelFont, fg, label);
        p.setOpacity(0.7);
        Css::drawCentered(&p, QRectF(x + SegPadX + labelW + SegCountGap, seg.top(), w, SegH), countFont, fg,
                          QString::number(m_counts.value(i)));
        p.setOpacity(1.0);
        x += w + SegGap;
    }
}

// ---------------------------------------------------------------- header ---

FluentHeader::FluentHeader(QWidget *parent)
    : QWidget(parent)
{
    m_profile = new FluentButton(FluentButton::Secondary, QString(), this);
    m_profile->setLeadingIcon(SlidersPath, 14, 2.0);
    m_profile->setTrailingChevron(true);
    m_profile->setText(Locale::tr(QStringLiteral("fluent.profile"))
                           .arg(Locale::tr(QStringLiteral("fluent.profile.default"))));
    connect(m_profile, &FluentButton::clicked, this, &FluentHeader::profileRequested);

    m_filter = new FluentSegmented({Locale::tr(QStringLiteral("segment.all")),
                                    Locale::tr(QStringLiteral("fluent.filter.changed")),
                                    Locale::tr(QStringLiteral("segment.active"))},
                                   this);
    connect(m_filter, &FluentSegmented::currentIndexChanged, this, &FluentHeader::filterChanged);

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, [this] {
        m_profile->setText(Locale::tr(QStringLiteral("fluent.profile"))
                               .arg(Locale::tr(QStringLiteral("fluent.profile.default"))));
        m_filter->setLabels({Locale::tr(QStringLiteral("segment.all")),
                             Locale::tr(QStringLiteral("fluent.filter.changed")),
                             Locale::tr(QStringLiteral("segment.active"))});
        relayout();
    });
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, &FluentHeader::relayout);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    relayout();
}

qreal FluentHeader::textBlockHeight() const
{
    return Css::line(Theme::sans(12), 1.4) + TextGap
           + Css::line(Theme::sans(28, Theme::Weight::SemiBold, -0.01), 1.15) + TextGap
           + Css::line(Theme::sans(13), 1.4);
}

QSize FluentHeader::sizeHint() const
{
    // The 16px gap under the text block is always there: it separates the filter from
    // the text when there is one, and the page from the text when there is not — the
    // pages that are not a tweak list bring only their own 2px of top padding.
    qreal h = PadTop + textBlockHeight() + BlockGap;
    if (m_controlsVisible)
        h += m_filter->height();
    return {0, qRound(h)};
}

void FluentHeader::setBreadcrumb(const QString &parent, const QString &page)
{
    m_parent = parent;
    m_page = page;
    update();
}

void FluentHeader::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    update();
}

void FluentHeader::setSubtitle(const QString &subtitle)
{
    if (m_subtitle == subtitle)
        return;
    m_subtitle = subtitle;
    update();
}

void FluentHeader::setControlsVisible(bool visible)
{
    if (m_controlsVisible == visible)
        return;
    m_controlsVisible = visible;
    m_profile->setVisible(visible);
    m_filter->setVisible(visible);
    relayout();
}

void FluentHeader::setFilterCounts(int all, int changed, int enabled)
{
    m_filter->setCounts({all, changed, enabled});
    relayout();
}

void FluentHeader::setFilterIndex(int index)
{
    m_filter->setCurrentIndex(index);
}

void FluentHeader::resizeEvent(QResizeEvent *e)
{
    if (e)
        QWidget::resizeEvent(e);
    relayout();
}

void FluentHeader::relayout()
{
    setFixedHeight(sizeHint().height());
    updateGeometry();
    // `align-items:flex-end`: the profile button sits on the text block's bottom edge.
    const qreal blockBottom = PadTop + textBlockHeight();
    m_profile->move(qRound(width() - PadX - m_profile->width()),
                    qRound(blockBottom - m_profile->height()));
    m_filter->move(qRound(PadX), qRound(blockBottom + BlockGap));
    update();
}

void FluentHeader::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont crumbFont = Theme::sans(12);
    const QFont titleFont = Theme::sans(28, Theme::Weight::SemiBold, -0.01);
    const QFont subFont = Theme::sans(13);
    const qreal crumbLine = Css::line(crumbFont, 1.4);
    const qreal titleLine = Css::line(titleFont, 1.15);
    const qreal subLine = Css::line(subFont, 1.4);

    const qreal right = m_controlsVisible ? m_profile->x() - GroupGap : width() - PadX;
    const qreal textW = qMax(0.0, right - PadX);

    qreal y = PadTop;
    // Breadcrumb: parent › page. The separator and the page take the parent's colour
    // and the page's own, exactly as the handoff's three spans do. A rail with a single
    // page — Genel Bakış, Paketler — would read "Genel Bakış › Genel Bakış", so the
    // parent is dropped where it is the page.
    qreal x = PadX;
    if (!m_parent.isEmpty() && m_parent != m_page) {
        const qreal baseline = Css::baseline(crumbFont, y, crumbLine);
        const qreal parentW = Css::textWidth(crumbFont, m_parent);
        Css::drawText(&p, QRectF(x, 0, parentW, height()), baseline, crumbFont, t.textMuted, m_parent);
        x += parentW + CrumbGap;
        const QString sep = QStringLiteral("›");
        const qreal sepW = Css::textWidth(crumbFont, sep);
        Css::drawText(&p, QRectF(x, 0, sepW, height()), baseline, crumbFont, t.textMuted, sep);
        x += sepW + CrumbGap;
        Css::drawText(&p, QRectF(x, 0, qMax(0.0, right - x), height()), baseline, crumbFont, t.textSec,
                      m_page, Qt::AlignLeft, true);
    } else if (!m_page.isEmpty()) {
        Css::drawText(&p, QRectF(x, 0, qMax(0.0, right - x), height()),
                      Css::baseline(crumbFont, y, crumbLine), crumbFont, t.textSec, m_page,
                      Qt::AlignLeft, true);
    }
    y += crumbLine + TextGap;

    Css::drawText(&p, QRectF(PadX, 0, textW, height()), Css::baseline(titleFont, y, titleLine), titleFont,
                  t.text, m_title, Qt::AlignLeft, true);
    y += titleLine + TextGap;

    Css::drawText(&p, QRectF(PadX, 0, textW, height()), Css::baseline(subFont, y, subLine), subFont,
                  t.textSec, m_subtitle, Qt::AlignLeft, true);
}
