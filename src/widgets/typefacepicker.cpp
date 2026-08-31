#include "typefacepicker.h"

#include "../css.h"
#include "../theme.h"
#include "segmentedcontrol.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QtMath>

namespace {
// The same two numbers the language grid uses, for the same reasons: the pill padding
// SegmentedControl and PillButton keep unscaled, and the one gallery gap.
constexpr qreal ChipPadX = 10.0;
constexpr qreal Gap = 10.0;
// px, the size every specimen is set at before the interface scale is applied. One size
// for all six so the comparison is between faces rather than between sizes.
constexpr qreal SpecimenPx = 11.5;
// A chip is never shorter than a segment of the filter control, and grows past it only if
// a face's own line box needs the room. The 8 is SegmentedControl's 3px padding top and
// bottom plus its 1px border on each edge, so a tall face lands on the same box model.
constexpr qreal SpecimenFrame = 8.0;
} // namespace

TypefacePicker::TypefacePicker(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    // Flows into the width it is handed, exactly like the language grid — the settings
    // page hands it the content column, the setup wizard a narrower one. Preferred and not
    // Fixed vertically for the reason spelled out in languagepicker.cpp: Fixed caps the
    // item at one row's height and clips the wrap.
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    measure();
    relayout();

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    // The specimens carry the interface scale, so a text-size change has to re-measure
    // them — this picker had no listener at all and was the one control on the settings
    // page that stayed its 1.0 size at every setting.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        measure();
        relayout();
        updateGeometry();
        update();
    });
}

void TypefacePicker::measure()
{
    m_chips.clear();

    qreal widest = 0.0;
    qreal tallest = 0.0;
    for (const Theme::Typeface &face : Theme::typefaces()) {
        Chip chip;
        chip.id = face.id;
        chip.name = face.name;

        // Registering the family here is what lets the chip be set in it — the face the
        // user has not chosen is otherwise never loaded.
        chip.font = QFont(Theme::loadTypeface(face.id));
        chip.font.setPointSizeF(SpecimenPx * Theme::fontScale() * 72.0 / Theme::logicalDpi());
        chip.font.setStyleStrategy(QFont::PreferAntialias);

        widest = qMax(widest, Css::textWidth(chip.font, chip.name));
        tallest = qMax(tallest, Css::normalLine(chip.font));

        m_chips.append(chip);
    }

    m_cell = widest + 2 * ChipPadX;
    m_chipH = qMax(SegmentedControl::controlHeight(), tallest + SpecimenFrame);
}

int TypefacePicker::columnsFor(int w) const
{
    return Css::flexColumns(w, m_cell, Gap, int(m_chips.size()));
}

void TypefacePicker::relayout()
{
    const int cols = columnsFor(width());
    if (cols <= 0)
        return;
    for (int i = 0; i < m_chips.size(); ++i) {
        const int col = i % cols;
        const int row = i / cols;
        m_chips[i].box = QRectF(col * (m_cell + Gap), row * (m_chipH + Gap), m_cell, m_chipH);
    }
}

QSize TypefacePicker::sizeHint() const
{
    const int n = int(m_chips.size());
    if (n == 0)
        return {0, qCeil(m_chipH)};
    return {qCeil(n * m_cell + (n - 1) * Gap), qCeil(m_chipH)};
}

int TypefacePicker::heightForWidth(int w) const
{
    const int cols = columnsFor(w);
    if (cols <= 0)
        return qCeil(m_chipH);
    const int rows = (int(m_chips.size()) + cols - 1) / cols;
    return qCeil(rows * m_chipH + (rows - 1) * Gap);
}

void TypefacePicker::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (e->size().width() != e->oldSize().width())
        relayout();
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
        Css::drawText(&p, chip.box,
                      Css::baseline(chip.font, chip.box.top() + (m_chipH - line) / 2.0, line),
                      chip.font, selected ? accentInk() : Color::TextSecondary(),
                      chip.name, Qt::AlignHCenter);
    }
}
