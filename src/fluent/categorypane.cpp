#include "categorypane.h"
#include "fluenticons.h"
#include "fluentsearchbox.h"
#include "../css.h"
#include "../i18n.h"
#include "../monitor.h"
#include "../theme.h"
#include "../widgets/smoothscrollarea.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {
constexpr qreal PadTop = 8.0;   // (48 - 32) / 2: the search box centred in the title band
constexpr qreal PadRight = 8.0;
constexpr qreal PadBottom = 12.0;
constexpr qreal PadLeft = 4.0;
constexpr qreal Gap = 8.0;
constexpr qreal RowGap = 2.0;
constexpr qreal RowPadX = 12.0;
constexpr qreal RowRadius = 4.0;
constexpr int RowIcon = 16;
constexpr qreal RowIconGap = 8.0;   // the row's own `gap:8px`
constexpr qreal BarW = 3.0;
constexpr qreal BarH = 16.0;
constexpr qreal BarTop = 10.0;
constexpr qreal HeadPadTop = 6.0;
constexpr qreal HeadPadBottom = 4.0;

constexpr qreal CardPad = 12.0;
constexpr qreal CardGap = 10.0;
constexpr qreal CardRadius = 6.0;
constexpr qreal MeterH = 3.0;
constexpr qreal MeterGap = 5.0;
constexpr qreal DotSize = 6.0;
constexpr qreal DotGap = 5.0;

QString gigabytes(quint64 bytes)
{
    const double gb = double(bytes) / (1024.0 * 1024.0 * 1024.0);
    return QString::number(gb, 'f', gb >= 100 ? 0 : 1);
}
} // namespace

// ------------------------------------------------------------------ pane row ---

PaneRow::PaneRow(const QString &id, const QString &label, const QString &count,
                 const Icons::Glyph *glyph, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
    , m_label(label)
    , m_count(count)
    , m_glyph(glyph)
{
    setFixedHeight(Height);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

void PaneRow::setSelected(bool on)
{
    if (m_selected == on)
        return;
    m_selected = on;
    update();
}

void PaneRow::setLabel(const QString &label)
{
    m_label = label;
    update();
}

void PaneRow::setCount(const QString &count)
{
    m_count = count;
    update();
}

void PaneRow::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void PaneRow::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void PaneRow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void PaneRow::mouseReleaseEvent(QMouseEvent *e)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (wasPressed && rect().contains(e->pos()))
            Q_EMIT activated(m_id);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void PaneRow::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_selected || m_hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_selected ? t.selected : t.subtleHover);
        p.drawRoundedRect(rect(), RowRadius, RowRadius);
    }
    if (m_selected) {
        p.setBrush(t.accent);
        p.drawRoundedRect(QRectF(0, BarTop, BarW, BarH), 2.0, 2.0);
    }

    const QFont countFont = Theme::mono(11);
    qreal countW = 0;
    if (!m_count.isEmpty()) {
        countW = Css::textWidth(countFont, m_count);
        Css::drawCentered(&p, QRectF(0, 0, width() - RowPadX, height()), countFont, t.textMuted, m_count,
                          Qt::AlignRight);
    }
    qreal labelX = RowPadX;
    if (m_glyph) {
        const QPixmap glyph = FluentIcons::draw(*m_glyph, m_selected ? t.text : t.textSec, RowIcon, 1.75,
                                                devicePixelRatioF());
        p.drawPixmap(QPointF(RowPadX, std::round((height() - RowIcon) / 2.0)), glyph);
        labelX += RowIcon + RowIconGap;
    }
    const qreal labelW = width() - labelX - RowPadX - (countW > 0 ? countW + Gap : 0.0);
    Css::drawCentered(&p, QRectF(labelX, 0, qMax(0.0, labelW), height()), Theme::sans(13), t.text,
                      m_label, Qt::AlignLeft, true);
}

// ---------------------------------------------------------------- status card ---

StatusCard::StatusCard(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    m_restore = QStringLiteral("—");
    setFixedHeight(sizeHint().height());
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        setFixedHeight(sizeHint().height());
        updateGeometry();
        update();
    });
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, qOverload<>(&QWidget::update));
}

QSize StatusCard::sizeHint() const
{
    const qreal head = Css::normalLine(Theme::sans(11, Theme::Weight::SemiBold));
    const qreal meter = Css::normalLine(Theme::sans(12)) + MeterGap + MeterH;
    const qreal foot = Css::normalLine(Theme::sans(12)) + Css::normalLine(Theme::sans(11));
    // header, three meters, the rule with its 10px above and below, the two-line footer
    return {0, qRound(2 + 2 * CardPad + head + CardGap + 3 * meter + 2 * CardGap + CardGap + 1
                      + CardGap + foot)};
}

void StatusCard::setSample(const Sample &s)
{
    m_live = true;
    m_cpu = qBound(0.0, s.cpuPercent, 100.0);
    m_ram = qBound(0.0, s.ramPercent, 100.0);
    m_disk = s.diskTotal > 0 ? qBound(0.0, 100.0 * double(s.diskUsed) / double(s.diskTotal), 100.0) : 0.0;
    m_cpuText = QStringLiteral("%1%").arg(qRound(m_cpu));
    m_ramText = QStringLiteral("%1 / %2 GB").arg(gigabytes(s.ramUsed), gigabytes(s.ramTotal));
    m_diskText = s.diskTotal > 0 ? QStringLiteral("%1 / %2 GB").arg(gigabytes(s.diskUsed), gigabytes(s.diskTotal))
                                 : QStringLiteral("—");
    update();
}

void StatusCard::setRestorePoint(const QString &text)
{
    m_restore = text.isEmpty() ? QStringLiteral("—") : text;
    update();
}

QRectF StatusCard::linkRect() const
{
    const QFont f = Theme::sans(12);
    const QString link = Locale::tr(QStringLiteral("fluent.status.create"));
    const qreal w = Css::textWidth(f, link);
    const qreal footH = Css::normalLine(Theme::sans(12)) + Css::normalLine(Theme::sans(11));
    const qreal bottom = height() - 1 - CardPad;
    return {width() - 1 - CardPad - w, bottom - footH, w, footH};
}

void StatusCard::mouseMoveEvent(QMouseEvent *e)
{
    const bool over = linkRect().contains(e->position());
    if (over != m_linkHovered) {
        m_linkHovered = over;
        setCursor(over ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void StatusCard::leaveEvent(QEvent *e)
{
    if (m_linkHovered) {
        m_linkHovered = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
    m_linkPressed = false;
    QWidget::leaveEvent(e);
}

void StatusCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_linkPressed = linkRect().contains(e->position());
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void StatusCard::mouseReleaseEvent(QMouseEvent *e)
{
    const bool pressed = m_linkPressed;
    m_linkPressed = false;
    if (e->button() == Qt::LeftButton) {
        e->accept();
        if (pressed && linkRect().contains(e->position()))
            Q_EMIT createRequested();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void StatusCard::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.setPen(QPen(t.cardBorder, 1.0));
    p.setBrush(t.card);
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), CardRadius, CardRadius);

    const qreal left = 1 + CardPad;
    const qreal right = width() - 1 - CardPad;
    const qreal innerW = right - left;
    qreal y = 1 + CardPad;

    // Header: the title, and "Canlı" with its dot at the right.
    const QFont headFont = Theme::sans(11, Theme::Weight::SemiBold);
    const qreal headH = Css::normalLine(headFont);
    Css::drawCentered(&p, QRectF(left, y, innerW, headH), headFont, t.textSec,
                      Css::upperTr(Locale::tr(QStringLiteral("fluent.status.title"))));
    if (m_live) {
        const QFont liveFont = Theme::sans(11, Theme::Weight::Medium);
        const QString live = Locale::tr(QStringLiteral("fluent.status.live"));
        const qreal liveW = Css::textWidth(liveFont, live);
        Css::drawCentered(&p, QRectF(left, y, innerW, headH), liveFont, t.ok, live, Qt::AlignRight);
        p.setPen(Qt::NoPen);
        p.setBrush(t.ok);
        p.drawEllipse(QRectF(right - liveW - DotGap - DotSize, y + (headH - DotSize) / 2.0, DotSize, DotSize));
    }
    y += headH + CardGap;

    // Three meters.
    const QFont labelFont = Theme::sans(12);
    const QFont valueFont = Theme::mono(11);
    const qreal labelH = Css::normalLine(labelFont);
    struct Meter { QString label; QString value; qreal pct; };
    const Meter meters[] = {
        {Locale::tr(QStringLiteral("fluent.status.cpu")), m_live ? m_cpuText : QStringLiteral("—"), m_cpu},
        {Locale::tr(QStringLiteral("fluent.status.ram")), m_live ? m_ramText : QStringLiteral("—"), m_ram},
        {Locale::tr(QStringLiteral("fluent.status.disk")), m_live ? m_diskText : QStringLiteral("—"), m_disk},
    };
    for (const Meter &m : meters) {
        Css::drawCentered(&p, QRectF(left, y, innerW, labelH), labelFont, t.textSec, m.label);
        Css::drawCentered(&p, QRectF(left, y, innerW, labelH), valueFont, t.text, m.value, Qt::AlignRight);
        y += labelH + MeterGap;
        p.setPen(Qt::NoPen);
        p.setBrush(t.track);
        p.drawRoundedRect(QRectF(left, y, innerW, MeterH), 2.0, 2.0);
        p.setBrush(t.accent);
        p.drawRoundedRect(QRectF(left, y, innerW * m.pct / 100.0, MeterH), 2.0, 2.0);
        y += MeterH + CardGap;
    }

    // The rule, then the footer.
    Css::hairline(&p, QRectF(left, y, innerW, 1), t.cardBorder);
    y += 1 + CardGap;
    const QFont footFont = Theme::sans(12);
    const QFont subFont = Theme::sans(11);
    const qreal footH = Css::normalLine(footFont);
    Css::drawCentered(&p, QRectF(left, y, innerW, footH), footFont, t.textSec,
                      Locale::tr(QStringLiteral("fluent.status.restore")));
    Css::drawCentered(&p, QRectF(left, y + footH, innerW, Css::normalLine(subFont)), subFont, t.textMuted,
                      m_restore, Qt::AlignLeft, true);

    const QString link = Locale::tr(QStringLiteral("fluent.status.create"));
    const QRectF lr = linkRect();
    Css::drawCentered(&p, lr, footFont, t.accent, link, Qt::AlignRight);
    if (m_linkHovered) {
        const qreal baseline = Css::centeredBaseline(footFont, lr);
        Css::hairline(&p, QRectF(lr.left(), baseline + 2, lr.width(), 1), t.accent);
    }
}

// ------------------------------------------------------------------- the pane ---

CategoryPane::CategoryPane(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(Theme::Fluent::PaneWidth);

    m_search = new FluentSearchBox(this);

    m_scroll = new SmoothScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_list = new QWidget(m_scroll);
    m_list->setAutoFillBackground(false);
    m_scroll->setWidget(m_list);

    m_status = new StatusCard(this);
    connect(m_status, &StatusCard::createRequested, this, &CategoryPane::restorePointRequested);

    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, &CategoryPane::relayout);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

void CategoryPane::setItems(const QString &heading, const QVector<Item> &items)
{
    for (PaneRow *row : std::as_const(m_rows)) {
        row->hide();
        row->deleteLater();
    }
    m_rows.clear();
    m_heading = Css::upperTr(heading);

    for (const Item &item : items) {
        auto *row = new PaneRow(item.id, item.label, item.count, item.glyph, m_list);
        row->setSelected(item.id == m_selected);
        connect(row, &PaneRow::activated, this, &CategoryPane::activated);
        row->show();
        m_rows.append(row);
    }
    relayout();
}

void CategoryPane::setSelected(const QString &id)
{
    m_selected = id;
    for (PaneRow *row : std::as_const(m_rows))
        row->setSelected(row->id() == id);
}

void CategoryPane::setCount(const QString &id, const QString &count)
{
    for (PaneRow *row : std::as_const(m_rows))
        if (row->id() == id)
            row->setCount(count);
}

void CategoryPane::setSample(const Sample &sample)
{
    m_status->setSample(sample);
}

void CategoryPane::setRestorePoint(const QString &text)
{
    m_status->setRestorePoint(text);
}

void CategoryPane::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    relayout();
}

void CategoryPane::relayout()
{
    const int contentW = qRound(width() - PadLeft - PadRight);
    m_search->setGeometry(qRound(PadLeft), qRound(PadTop), contentW, m_search->sizeHint().height());

    const int statusH = m_status->sizeHint().height();
    m_status->setGeometry(qRound(PadLeft), qRound(height() - PadBottom - statusH), contentW, statusH);

    // The heading has a strip of its own between the search box and the list, and the
    // scroll area starts under it. It used to be painted on the pane behind the viewport's
    // first rows' worth of space, with the list's content offset by the same height — which
    // held until the list scrolled, when the rows moved up into that space and were drawn
    // over the heading by the viewport, since a child paints over its parent.
    const qreal headH = HeadPadTop + Css::normalLine(Theme::sans(11, Theme::Weight::SemiBold, 0.04))
                        + HeadPadBottom;
    const int listTop = qRound(PadTop + m_search->height() + Gap + headH);
    const int listH = qMax(0, qRound(m_status->y() - Gap) - listTop);
    m_scroll->setGeometry(qRound(PadLeft), listTop, contentW, listH);

    const auto layoutRows = [this](int rowW) {
        int y = 0;
        for (int i = 0; i < m_rows.size(); ++i) {
            if (i > 0)
                y += qRound(RowGap);
            m_rows.at(i)->setGeometry(0, y, rowW, PaneRow::Height);
            y += PaneRow::Height;
        }
        return y;
    };
    int rowW = contentW;
    int contentH = layoutRows(rowW);
    if (contentH > listH) {
        rowW = qMax(0, contentW - Theme::Metric::ScrollBarWidth);
        contentH = layoutRows(rowW);
    }
    m_list->setFixedSize(rowW, contentH);
    m_list->update();
    update();
}

void CategoryPane::paintEvent(QPaintEvent *)
{
    // The heading sits in its own strip between the search box and the scroll area (see
    // relayout()), so the list is clipped beneath it rather than drawn across it.
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const QFont f = Theme::sans(11, Theme::Weight::SemiBold, 0.04);
    const qreal h = Css::normalLine(f);
    const qreal top = m_search->y() + m_search->height() + Gap + HeadPadTop;
    Css::drawCentered(&p, QRectF(PadLeft + RowPadX, top, m_scroll->width() - 2 * RowPadX, h),
                      f, t.textMuted, m_heading, Qt::AlignLeft, true);
}
