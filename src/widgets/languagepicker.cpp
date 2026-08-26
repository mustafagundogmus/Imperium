#include "languagepicker.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr qreal ChipH = 26.0;
constexpr qreal ChipPadX = 10.0;
constexpr qreal Gap = 6.0;
constexpr qreal RowGap = 6.0;
constexpr int PerRow = 5;   // five languages per row, two rows for the ten the build carries
} // namespace

LanguagePicker::LanguagePicker(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    rebuild();
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, qOverload<>(&QWidget::update));
    // Every chip's rectangle is measured from Font::segment(), which carries the interface
    // scale, so a face swap or a text-size change has to re-measure them. Without this the
    // chips kept the widths of the size they were built at: at "Büyük" the labels ran past
    // their outlines, at "Küçük" they floated inside them.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        rebuild();
        update();
    });
}

void LanguagePicker::rebuild()
{
    m_chips.clear();

    QFont font = Theme::Font::segment();

    qreal x = 0.0, y = 0.0;
    int col = 0;
    for (const Locale::Language &lang : Locale::languages()) {
        Chip chip;
        chip.id = lang.id;
        chip.name = lang.nativeName;

        const qreal w = Css::textWidth(font, chip.name) + 2 * ChipPadX;
        chip.box = QRectF(x, y, w, ChipH);
        x += w + Gap;

        m_chips.append(chip);

        if (++col >= PerRow) {
            col = 0;
            x = 0.0;
            y += ChipH + RowGap;
        }
    }

    setFixedSize(sizeHint());
}

QSize LanguagePicker::sizeHint() const
{
    if (m_chips.isEmpty())
        return {0, qRound(ChipH)};

    qreal maxRight = 0.0, maxBottom = 0.0;
    for (const Chip &c : m_chips) {
        maxRight = qMax(maxRight, c.box.right());
        maxBottom = qMax(maxBottom, c.box.bottom());
    }
    return {qRound(maxRight), qRound(maxBottom)};
}

int LanguagePicker::indexAt(const QPointF &pos) const
{
    for (int i = 0; i < m_chips.size(); ++i)
        if (m_chips.at(i).box.contains(pos))
            return i;
    return -1;
}

void LanguagePicker::mouseMoveEvent(QMouseEvent *e)
{
    const int hit = indexAt(e->position());
    if (hit != m_hovered) {
        m_hovered = hit;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void LanguagePicker::leaveEvent(QEvent *e)
{
    if (m_hovered != -1 || m_pressed != -1) {
        m_hovered = m_pressed = -1;
        update();
    }
    QWidget::leaveEvent(e);
}

void LanguagePicker::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_pressed = indexAt(e->position());
    update();
}

void LanguagePicker::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    const int hit = indexAt(e->position());
    if (hit >= 0 && hit == m_pressed)
        Q_EMIT picked(m_chips.at(hit).id);
    m_pressed = -1;
    update();
}

void LanguagePicker::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QString current = Locale::language();
    const QFont font = Font::segment();
    const qreal line = Css::normalLine(font);

    for (int i = 0; i < m_chips.size(); ++i) {
        const Chip &chip = m_chips.at(i);
        const bool selected = chip.id == current;

        if (selected) {
            p.setPen(QPen(accent(), 1.0));
            p.setBrush(accentSoft());
        } else if (m_hovered == i || m_pressed == i) {
            p.setPen(QPen(Color::BorderControl(), 1.0));
            p.setBrush(Color::SurfaceHover());
        } else {
            p.setPen(QPen(Color::BorderControl(), 1.0));
            p.setBrush(Color::Surface());
        }
        p.drawRoundedRect(chip.box.adjusted(0.5, 0.5, -0.5, -0.5),
                          Metric::ControlRadius, Metric::ControlRadius);

        Css::drawText(&p, chip.box, Css::baseline(font, chip.box.top() + (ChipH - line) / 2.0, line),
                      font, selected ? accentInk() : Color::TextSecondary(),
                      chip.name, Qt::AlignHCenter);
    }
}
