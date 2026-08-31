#include "settingrow.h"

#include "../css.h"
#include "../theme.h"

#include <QFontMetricsF>
#include <QLayout>
#include <QPainter>

namespace {

constexpr qreal BorderW = 1.0;
constexpr qreal PadX = 6.0;
const qreal LeadingCol = Theme::Metric::ToggleWidth;
constexpr qreal ColGap = 12.0;
constexpr qreal TrailingGap = 16.0;
constexpr qreal TextGap = 1.0;
// A Below row's caption to the gallery under it. The same 10 the galleries put between
// their own cells, and the same 10 SectionHeader puts between its label and its rule — the
// page is deliberately built from one gap.
constexpr qreal BlockGap = 10.0;
// How many lines a Below row's description may run to. The captions on the settings page
// are sentences rather than the half-line labels a list row carries: measured against the
// column it now gets, the language one is 1136px of French at the default text size and
// 1439px at the largest, so on one line it is elided whatever the layout does. Two is the
// cap — a third would push the gallery it labels out of sight, and nothing in the table
// needs one. Leading and Trailing rows stay at exactly one line, because they are the
// tweak list's own rhythm and four other pages are built on it.
constexpr int MaxDescLines = 2;

/// Breaks \a text into at most \a maxLines lines that each fit \a width, cutting at spaces
/// so words stay whole. The last line is elided if what is left of the text does not fit.
QStringList wrapDesc(const QFont &f, const QString &text, qreal width, int maxLines)
{
    QStringList lines;
    if (text.isEmpty() || width <= 0.0 || maxLines <= 0)
        return lines;

    const QFontMetricsF fm(f);
    QString rest = text;
    while (!rest.isEmpty()) {
        const bool lastLine = lines.size() + 1 >= maxLines;
        if (fm.horizontalAdvance(rest) <= width) {
            lines << rest;
            break;
        }
        if (lastLine) {
            lines << fm.elidedText(rest, Qt::ElideRight, width);
            break;
        }

        // The longest prefix ending on a space that still fits.
        int cut = -1;
        for (int i = rest.indexOf(QLatin1Char(' ')); i > 0;
             i = rest.indexOf(QLatin1Char(' '), i + 1)) {
            if (fm.horizontalAdvance(rest.left(i)) > width)
                break;
            cut = i;
        }
        if (cut <= 0) {
            // A single word wider than the column. Nothing to break on, so trim it.
            lines << fm.elidedText(rest, Qt::ElideRight, width);
            break;
        }
        lines << rest.left(cut);
        rest = rest.mid(cut + 1);
    }
    return lines;
}

} // namespace

SettingRow::SettingRow(const QString &name, const QString &desc, QWidget *control,
                       Placement placement, QWidget *parent)
    : QWidget(parent)
    , m_name(name)
    , m_desc(desc)
    , m_control(control)
    , m_placement(placement)
{
    if (m_control)
        m_control->setParent(this);

    if (m_placement == Below) {
        // A Below row cannot pin its height here: it does not know one until it knows how
        // wide a column it got. It answers heightForWidth() instead, which the QVBoxLayout
        // holding it asks on its behalf — QWidget::hasHeightForWidth() consults a widget's
        // layout, so the question travels all the way up to the scroll area.
        //
        // Preferred vertically, not Fixed. Fixed would cap the row at sizeHint().height()
        // however tall heightForWidth() says it is, and the slack a Preferred item could
        // otherwise absorb goes to the stretch the page keeps at the bottom of its layout.
        QSizePolicy policy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    } else {
        setFixedHeight(sizeHint().height());
    }

    // Every metric here comes out of the font and the density flag, so either of them
    // changing means measuring again — see the note in tweakrow.cpp about compactChanged
    // having spent three releases with no listener at all.
    const auto remeasure = [this] {
        m_linesDirty = true;   // the wrap was measured in the old font
        if (m_placement == Below)
            invalidateLayoutChain();
        else
            setFixedHeight(sizeHint().height());
        updateGeometry();
        positionControl();
        update();
    };
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, remeasure);
    connect(Theme::notifier(), &Theme::Notifier::compactChanged, this, remeasure);

    positionControl();
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

void SettingRow::invalidateLayoutChain()
{
    // A Below row does not carry its height, it answers heightForWidth() with it — and
    // QBoxLayout caches that answer against the width it last asked for. What changed here
    // is the font, not the width, so every layout between this row and the scroll area
    // hands back the height it worked out for the old metrics, and updateGeometry()
    // invalidates only the first of them. The cascade does clear itself, one layout per
    // spin of the event loop, which sounds harmless and is not: measured on the real tree,
    // pressing "Büyük" left the five galleries drawn at the old height for the frame
    // straight after the press — the theme grid painted past the bottom of its own row —
    // and going back to "Normal" left the row 23px taller than it should have been until
    // something else happened to disturb the layout. Walking the chain makes the new
    // height the first one drawn.
    //
    // Leading and Trailing rows never reach here. setFixedHeight() changes their height
    // outright instead of asking a layout to work it out, so they have no cached answer to
    // go stale — which is why the four other pages built on SettingRow pay nothing for it.
    for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
        if (QLayout *layout = w->layout())
            layout->invalidate();
        if (w->isWindow())
            break;
    }
}

int SettingRow::rowHeight()
{
    return qRound(2 * (BorderW + Css::rowPadY())
                  + Css::rowNameLine() + TextGap + Css::rowDescLine());
}

const QStringList &SettingRow::descLines(qreal textW) const
{
    // Cached, because this is asked three times per layout pass and again on every paint,
    // and finding the break point costs one text measurement per word. The settings page
    // scrolls, so re-shaping a two-sentence caption at every frame is not free. Only three
    // things can change the answer: the text, the column, and the font — the first two are
    // checked here and the third invalidates the cache from the remeasure handler.
    if (m_linesDirty || m_linesWidth != textW) {
        // Only a Below caption wraps; everywhere else the description is one line that
        // elides, which is what keeps a settings row exactly as tall as a tweak row.
        m_lines = wrapDesc(Theme::Font::tweakDesc(), m_desc, textW,
                           m_placement == Below ? MaxDescLines : 1);
        m_linesWidth = textW;
        m_linesDirty = false;
    }
    return m_lines;
}

qreal SettingRow::textBlock(qreal textW) const
{
    const int lines = qMax(1, int(descLines(textW).size()));
    return Css::rowNameLine() + TextGap + lines * Css::rowDescLine();
}

/// Width the caption has to lay out in, for a row \a w wide.
qreal SettingRow::textWidthFor(int w) const
{
    if (m_placement == Leading)
        return w - (2 * BorderW + 2 * PadX + LeadingCol + ColGap);
    if (m_placement == Trailing && m_control)
        return w - (2 * BorderW + 2 * PadX) - m_control->width() - TrailingGap;
    return w - 2 * (BorderW + PadX);
}

int SettingRow::controlHeightFor(int available) const
{
    if (!m_control)
        return 0;
    // A gallery that flows takes the whole column and wraps; a control with a width of its
    // own — the segmented text-size control, the swatch strip — is left at that width by
    // the qMin, since a fixed size shows up as an equal maximum.
    const int w = qMin(available, m_control->maximumWidth());
    return m_control->hasHeightForWidth() ? m_control->heightForWidth(w)
                                          : m_control->sizeHint().height();
}

int SettingRow::heightFor(int w) const
{
    if (m_placement == Below) {
        // Twice the padding: see the note in the header about the gap between two blocks
        // having to beat the gap inside one.
        const qreal frame = 2 * (BorderW + 2 * Css::rowPadY());
        const int inner = qMax(0, w - int(2 * (BorderW + PadX)));
        return qRound(frame + textBlock(textWidthFor(w)) + BlockGap) + controlHeightFor(inner);
    }

    int h = rowHeight();
    if (m_control)
        h = qMax(h, int(m_control->sizeHint().height() + 2 * (BorderW + Css::rowPadY())));
    return h;
}

QSize SettingRow::sizeHint() const
{
    return {0, heightFor(width())};
}

void SettingRow::setName(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    updateGeometry();
    update();
}

void SettingRow::setDesc(const QString &desc)
{
    if (m_desc == desc)
        return;
    m_desc = desc;
    m_linesDirty = true;
    // A Below caption that gains or loses its second line changes the row's height, so the
    // layout has to be told. The rows that update their description while the page is open
    // — the update check, the restore point, the TrustedInstaller launches — are all
    // Trailing and one line, where this costs nothing.
    updateGeometry();
    update();
}

void SettingRow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    positionControl();
}

void SettingRow::positionControl()
{
    if (!m_control)
        return;

    if (m_placement == Below) {
        const int inner = qMax(0, width() - int(2 * (BorderW + PadX)));
        const int w = qMin(inner, m_control->maximumWidth());
        const int y = qRound(BorderW + 2 * Css::rowPadY()
                             + textBlock(textWidthFor(width())) + BlockGap);
        // setGeometry rather than resize+move: it clamps to the control's own minimum and
        // maximum on its way through, which is what leaves a fixed-size control alone.
        m_control->setGeometry(qRound(BorderW + PadX), y, w, controlHeightFor(inner));
        return;
    }

    const QSize hint = m_control->sizeHint();
    m_control->resize(hint.isEmpty() ? m_control->size() : hint);

    const int y = qRound((height() - m_control->height()) / 2.0);
    if (m_placement == Leading)
        m_control->move(qRound(BorderW + PadX), y);
    else
        m_control->move(qRound(width() - BorderW - PadX - m_control->width()), y);
}

void SettingRow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal textX = m_placement == Leading ? BorderW + PadX + LeadingCol + ColGap
                                               : BorderW + PadX;
    qreal textRight = width() - (BorderW + PadX);
    if (m_placement == Trailing && m_control)
        textRight = m_control->x() - TrailingGap;

    const qreal textW = textRight - textX;
    if (textW <= 0)
        return;

    // The text block is centred on the row so a taller control does not drag it upwards —
    // except under Below, where the control is not beside the text but under it, and the
    // caption belongs at the top of its own block.
    const qreal block = textBlock(textW);
    const qreal top = m_placement == Below ? BorderW + 2 * Css::rowPadY()
                                           : (height() - block) / 2.0;

    const QFont &nameFont = Font::tweakName();
    const QFont &descFont = Font::tweakDesc();
    const QRectF box(textX, 0, textW, height());

    Css::drawText(&p, box, Css::baseline(nameFont, top, Css::rowNameLine()),
                  nameFont, Color::TextPrimary(), m_name, Qt::AlignLeft, true);

    // wrapDesc() has already cut each line to the column, so nothing here elides a second
    // time; a Leading or Trailing row simply gets a one-line list back.
    qreal lineTop = top + Css::rowNameLine() + TextGap;
    const QStringList lines = descLines(textW);
    for (const QString &text : lines) {
        Css::drawText(&p, box, Css::baseline(descFont, lineTop, Css::rowDescLine()),
                      descFont, Color::TextDesc(), text, Qt::AlignLeft);
        lineTop += Css::rowDescLine();
    }
}
