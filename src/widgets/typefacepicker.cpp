#include "typefacepicker.h"

#include "../css.h"
#include "../theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr qreal ChipH = 26.0;
constexpr qreal ChipPadX = 10.0;
constexpr qreal Gap = 6.0;
constexpr qreal ChipSize = 11.5;   // px, the size the specimen is set at
} // namespace

TypefacePicker::TypefacePicker(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    rebuild();
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

void TypefacePicker::rebuild()
{
    m_chips.clear();

    qreal x = 0.0;
    for (const Theme::Typeface &face : Theme::typefaces()) {
        Chip chip;
        chip.id = face.id;
        chip.name = face.name;

        // Registering the family here is what lets the chip be set in it — the face the
        // user has not chosen is otherwise never loaded.
        chip.font = QFont(Theme::loadTypeface(face.id));
        chip.font.setPointSizeF(ChipSize * 72.0 / Theme::logicalDpi());
        chip.font.setStyleStrategy(QFont::PreferAntialias);

        const qreal w = Css::textWidth(chip.font, chip.name) + 2 * ChipPadX;
        chip.box = QRectF(x, 0.0, w, ChipH);
        x += w + Gap;

        m_chips.append(chip);
    }

    setFixedSize(sizeHint());
}

QSize TypefacePicker::sizeHint() const
{
    if (m_chips.isEmpty())
        return {0, qRound(ChipH)};
    return {qRound(m_chips.last().box.right()), qRound(ChipH)};
}

int TypefacePicker::indexAt(const QPointF &pos) const
{
    for (int i = 0; i < m_chips.size(); ++i)
        if (m_chips.at(i).box.contains(pos))
            return i;
    return -1;
}

void TypefacePicker::mouseMoveEvent(QMouseEvent *e)
{
    const int hit = indexAt(e->position());
    if (hit != m_hovered) {
        m_hovered = hit;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void TypefacePicker::leaveEvent(QEvent *e)
{
    if (m_hovered != -1 || m_pressed != -1) {
        m_hovered = m_pressed = -1;
        update();
    }
    QWidget::leaveEvent(e);
}

void TypefacePicker::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_pressed = indexAt(e->position());
    update();
}

void TypefacePicker::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    const int hit = indexAt(e->position());
    if (hit >= 0 && hit == m_pressed)
        Q_EMIT picked(m_chips.at(hit).id);
    m_pressed = -1;
    update();
}

void TypefacePicker::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QString current = typeface();

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

        const qreal line = Css::normalLine(chip.font);
        Css::drawText(&p, chip.box, Css::baseline(chip.font, chip.box.top() + (ChipH - line) / 2.0, line),
                      chip.font, selected ? accentInk() : Color::TextSecondary(),
                      chip.name, Qt::AlignHCenter);
    }
}
