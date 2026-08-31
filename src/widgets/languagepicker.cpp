#include "languagepicker.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"
#include "segmentedcontrol.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QtMath>

namespace {
// The pill padding SegmentedControl and PillButton already use, and unscaled like theirs:
// in this app the type grows with the interface scale and the padding around it does not.
constexpr qreal ChipPadX = 10.0;
// One gallery gap, on both axes, shared with the theme cards and the accent swatches. The
// picker used to keep 6px between chips and 6px between rows while the strip beside it
// kept 10, which is the sort of one-off spacing the page was reported for.
constexpr qreal Gap = 10.0;
} // namespace

LanguagePicker::LanguagePicker(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    // The strip flows into whatever width it is handed and reports the height that leaves.
    // That is what lets the settings page give it the whole content column and the setup
    // wizard give it a narrower one, without either of them having to know the shape.
    //
    // Preferred vertically rather than Fixed, and the difference is not cosmetic: a Fixed
    // vertical policy makes qSmartMaxSize cap the item at sizeHint().height(), which is the
    // height of a single row, and a QBoxLayout then clips the wrap it asked for. Measured
    // in the setup wizard, whose 758px column takes ten chips to two rows: the picker was
    // handed 22px and the second row vanished.
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    measure();
    relayout();

    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
    // The names are written in their own languages, so a language change moves the
    // selection and nothing else.
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, qOverload<>(&QWidget::update));
    // Every chip's rectangle is measured from Font::segment(), which carries the interface
    // scale, so a face swap or a text-size change has to re-measure them. Without this the
    // chips kept the widths of the size they were built at: at "Büyük" the labels ran past
    // their outlines, at "Küçük" they floated inside them.
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
        measure();
        relayout();
        updateGeometry();
        update();
    });
}

void LanguagePicker::measure()
{
    m_chips.clear();

    const QFont font = Theme::Font::segment();

    // One width for every chip, taken from the widest native name. Ten names of ten
    // different lengths laid end to end is precisely the ragged block this page was
    // reported for; a uniform cell turns the same ten into a grid whose columns line up,
    // and it costs only the difference between "Polski" and "Português".
    qreal widest = 0.0;
    for (const Locale::Language &lang : Locale::languages()) {
        Chip chip;
        chip.id = lang.id;
        chip.name = lang.nativeName;
        widest = qMax(widest, Css::textWidth(font, chip.name));
        m_chips.append(chip);
    }

    m_cell = widest + 2 * ChipPadX;
    // The app has one pill height and it is derived from the type, so a chip ends up
    // exactly as tall as a segment of the filter control and a ghost button.
    m_chipH = SegmentedControl::controlHeight();
}

int LanguagePicker::columnsFor(int w) const
{
    return Css::flexColumns(w, m_cell, Gap, int(m_chips.size()));
}

void LanguagePicker::relayout()
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

QSize LanguagePicker::sizeHint() const
{
    const int n = int(m_chips.size());
    if (n == 0)
        return {0, qCeil(m_chipH)};
    // The shape it would rather have: every language on one line. Anything narrower is
    // handled by the wrap, so this is a preference and not a demand.
    return {qCeil(n * m_cell + (n - 1) * Gap), qCeil(m_chipH)};
}

int LanguagePicker::heightForWidth(int w) const
{
    const int cols = columnsFor(w);
    if (cols <= 0)
        return qCeil(m_chipH);
    const int rows = (int(m_chips.size()) + cols - 1) / cols;
    return qCeil(rows * m_chipH + (rows - 1) * Gap);
}

void LanguagePicker::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (e->size().width() != e->oldSize().width())
        relayout();
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

        Css::drawText(&p, chip.box,
                      Css::baseline(font, chip.box.top() + (m_chipH - line) / 2.0, line),
                      font, selected ? accentInk() : Color::TextSecondary(),
                      chip.name, Qt::AlignHCenter);
    }
}
