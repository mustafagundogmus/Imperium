#include "dialog.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"
#include "buttons.h"

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>
#include <QScreen>
#include <QScrollBar>

namespace {

using namespace Theme;

// The ApplyOverlay card's own width and padding. Two modal cards that differed by a few
// pixels would be worse than either choice on its own.
constexpr qreal CardWidth = 460.0;
constexpr qreal Pad = 16.0;

// Room around the card for the scrim. The dialog is a top-level window, so the scrim is
// only as wide as the window; sizing it to the parent means the darkening covers the
// application rather than a rectangle floating in the middle of it.
constexpr qreal GapTitleToBody = 8.0;
constexpr qreal GapBodyToDetail = 12.0;
constexpr qreal GapToButtons = 16.0;
constexpr int ButtonGap = 8;

// The detail panel is capped rather than allowed to grow: an action script can be forty
// lines, and a dialog taller than the window it interrupts is not a dialog any more.
constexpr int DetailMaxHeight = 200;
constexpr int DetailPad = 10;
/// QPlainTextEdit's own margin inside its viewport, set explicitly below rather than
/// left at the default so this number and the widget agree.
constexpr int DocumentMargin = 2;

/// Greedy word wrap at \a width. QPainter can wrap on its own, but the height has to be
/// known before the window is sized, and measuring it twice through two different code
/// paths is how the measured height and the painted height drift apart.
QStringList wrap(const QFont &font, const QString &text, qreal width)
{
    const QFontMetricsF fm(font);
    QStringList out;
    // Honour the newlines the callers put in deliberately — the action confirmations
    // separate the description from the note with one.
    const QStringList paragraphs = text.split(QLatin1Char('\n'));
    for (const QString &paragraph : paragraphs) {
        if (paragraph.trimmed().isEmpty()) {
            out << QString();
            continue;
        }
        QString line;
        const QStringList words = paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &word : words) {
            const QString candidate = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
            if (!line.isEmpty() && fm.horizontalAdvance(candidate) > width) {
                out << line;
                line = word;
            } else {
                line = candidate;
            }
        }
        if (!line.isEmpty())
            out << line;
    }
    return out;
}

} // namespace

Dialog::Dialog(QWidget *parent, const QString &title, const QString &body,
               const QString &acceptText, const QString &rejectText, const QString &detail)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , m_title(title)
    , m_body(body)
    , m_detail(detail)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setModal(true);

    m_accept = new PillButton(PillButton::Accent, acceptText, this);
    connect(m_accept, &PillButton::clicked, this, &QDialog::accept);

    // One button means a statement, not a question — inform() passes an empty reject
    // label and gets a card with a single control, the way the apply result card has.
    if (!rejectText.isEmpty()) {
        m_reject = new PillButton(PillButton::Ghost, rejectText, this);
        connect(m_reject, &PillButton::clicked, this, &QDialog::reject);
    }

    if (!m_detail.isEmpty()) {
        m_toggle = new PillButton(PillButton::Ghost,
                                  Locale::tr(QStringLiteral("dialog.details")), this);
        connect(m_toggle, &PillButton::clicked, this, [this] {
            m_detailOpen = !m_detailOpen;
            m_toggle->setText(Locale::tr(m_detailOpen ? QStringLiteral("dialog.details.hide")
                                                      : QStringLiteral("dialog.details")));
            m_detailView->setVisible(m_detailOpen);
            relayout();
        });

        // A real text view rather than painted lines: this is the one block in the app a
        // user has a reason to select and copy, and it is the block whose whole purpose
        // is that it can be checked against something else.
        m_detailView = new QPlainTextEdit(m_detail, this);
        m_detailView->setReadOnly(true);
        m_detailView->setFrameShape(QFrame::NoFrame);
        m_detailView->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_detailView->setFont(Font::infoValueMono());
        // Painted through the tokens like everything else. The viewport is left
        // transparent so the inset panel drawn in paintEvent shows through it, which is
        // what makes the block read as recessed rather than as a pasted-in text box.
        m_detailView->setStyleSheet(
            QStringLiteral("QPlainTextEdit { background: transparent; border: none;"
                           " color: %1; selection-background-color: %2;"
                           " selection-color: %3; }")
                .arg(Color::TextMono().name(), Theme::accent().name(),
                     Color::OnAccent().name()));
        m_detailView->document()->setDocumentMargin(DocumentMargin);
        m_detailView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_detailView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_detailView->hide();
    }

    relayout();
}

void Dialog::relayout()
{
    const qreal textWidth = CardWidth - 2 * Pad;

    const QFont &titleFont = Font::blockTitle();
    const QFont &bodyFont = Font::tweakDesc();
    const qreal titleLine = Css::normalLine(titleFont);
    const qreal bodyLine = Css::normalLine(bodyFont);

    m_bodyLines = wrap(bodyFont, m_body, textWidth);

    qreal h = Pad + titleLine + GapTitleToBody + m_bodyLines.size() * bodyLine;

    // The toggle adds no row of its own: it sits on the button row, at the other end of
    // it. Counting its height here as well left a band of empty card above the buttons.
    if (m_detailOpen && m_detailView) {
        // The panel is always a whole number of lines tall.
        //
        // That is the rule, and it is the one that took three attempts to get right. The
        // cap is a pixel count, the line height is a font metric, and the two do not
        // divide: at the mono face's 22px line, a nine-line script wants 202 and the cap
        // is 200, so the panel showed eight lines and the top half of a ninth. A half
        // line is worse than a scroll bar — it reads as a rendering fault rather than as
        // "there is more". So the cap is converted to a number of lines first, and the
        // height follows from that.
        //
        // Two things this deliberately does not do. It does not measure through
        // document()->size(): QPlainTextEdit uses QPlainTextDocumentLayout, whose
        // documentSize() reports height as a count of blocks rather than pixels, which
        // collapsed the panel to a single line. And it does not build its own
        // QFontMetricsF from Font::infoValueMono(): the widget resolves its own font, and
        // measuring one font while another is laid out is what clipped the last line.
        // The row height comes from the layout that will draw it, not from a font metric
        // about it. blockBoundingRect() is what QPlainTextDocumentLayout itself uses to
        // place each block; fontMetrics().lineSpacing() was a pixel short of it here,
        // which is a whole line short over nine of them.
        auto *layout = qobject_cast<QPlainTextDocumentLayout *>(
            m_detailView->document()->documentLayout());
        const int lineH = layout
            ? qMax(1, int(layout->blockBoundingRect(
                              m_detailView->document()->firstBlock()).height() + 0.5))
            : qMax(1, m_detailView->fontMetrics().lineSpacing());
        const int blocks = qMax(1, m_detailView->document()->blockCount());
        const int maxLines = qMax(1, (DetailMaxHeight - 2 * DocumentMargin) / lineH);
        int wanted = qMin(blocks, maxLines) * lineH + 2 * DocumentMargin;

        // A horizontal scroll bar takes its height out of the bottom of the viewport, so
        // a panel measured without it hides the script's last line behind the bar. The
        // lines are not wrapped — a wrapped PowerShell pipeline reads as a different
        // script — so the bar appears whenever any line is wider than the panel, and its
        // height is part of the measurement rather than something discovered afterwards.
        const int viewWidth = int(CardWidth - 2 * Pad - 2 * DetailPad) - 2 * DocumentMargin;
        qreal widest = 0.0;
        const QFontMetricsF fm(m_detailView->font());
        const QStringList lines = m_detail.split(QLatin1Char('\n'));
        for (const QString &line : lines)
            widest = qMax(widest, fm.horizontalAdvance(line));
        if (widest > viewWidth)
            wanted += m_detailView->horizontalScrollBar()->sizeHint().height();

        m_detailView->setFixedHeight(wanted);
        h += GapBodyToDetail + m_detailView->height() + 2 * DetailPad;
    }

    h += GapToButtons + m_accept->sizeHint().height() + Pad;

    // Stored rather than recomputed by cardRect(). Measuring the card in two places is
    // how the painted card and the placed controls drift apart, and this height depends
    // on a wrap, a font and a toggle state — three things that all change at once.
    m_cardHeight = h;

    // The scrim is the parent window's rectangle, so the card floats in the middle of the
    // application rather than in the middle of nothing.
    if (QWidget *owner = parentWidget() ? parentWidget()->window() : nullptr)
        setFixedSize(owner->size());
    else
        setFixedSize(int(CardWidth) + 80, int(h + 0.5) + 80);

    const QRectF card = cardRect();

    // --- controls, bottom-right, accept last -------------------------------
    qreal y = card.bottom() - Pad - m_accept->height();
    qreal x = card.right() - Pad - m_accept->width();
    m_accept->move(int(x), int(y));
    if (m_reject) {
        x -= ButtonGap + m_reject->width();
        m_reject->move(int(x), int(y + (m_accept->height() - m_reject->height()) / 2.0));
    }

    if (m_toggle) {
        // Left-aligned, opposite the answers: it is not one of them.
        m_toggle->move(int(card.left() + Pad),
                       int(y + (m_accept->height() - m_toggle->height()) / 2.0));
        // The panel's own top; the view sits DetailPad inside it on every side, which is
        // what paintEvent draws back out from the view's geometry.
        const qreal panelTop = card.top() + Pad + titleLine + GapTitleToBody
                               + m_bodyLines.size() * bodyLine + GapBodyToDetail;
        if (m_detailOpen) {
            m_detailView->setGeometry(int(card.left() + Pad + DetailPad),
                                      int(panelTop + DetailPad),
                                      int(CardWidth - 2 * Pad - 2 * DetailPad),
                                      m_detailView->height());
        }
    }

    centreOnParent();
    update();
}

QRectF Dialog::cardRect() const
{
    // The one height relayout() measured. Centred in whatever the window turned out to
    // be: the parent's rectangle when there is one, the card plus its scrim margin when
    // there is not.
    return QRectF((width() - CardWidth) / 2.0, (height() - m_cardHeight) / 2.0,
                  CardWidth, m_cardHeight);
}

void Dialog::centreOnParent()
{
    if (QWidget *owner = parentWidget() ? parentWidget()->window() : nullptr) {
        move(owner->mapToGlobal(QPoint(0, 0)));
        return;
    }
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        move(available.center() - QPoint(width() / 2, height() / 2));
    }
}

void Dialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // The scrim, at the alpha ApplyOverlay uses for the same job.
    QColor scrim = Color::Window();
    scrim.setAlpha(226);
    p.fillRect(rect(), scrim);

    const QRectF card = cardRect();
    p.setPen(QPen(Color::TileBorder(), 1.0));
    p.setBrush(Color::Tile());
    p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5),
                      Metric::ControlRadius, Metric::ControlRadius);

    const QRectF inner(card.left() + Pad, 0, CardWidth - 2 * Pad, height());

    const QFont &titleFont = Font::blockTitle();
    const qreal titleLine = Css::normalLine(titleFont);
    qreal y = card.top() + Pad;
    Css::drawText(&p, inner, Css::baseline(titleFont, y, titleLine), titleFont,
                  Color::TextPrimary(), m_title);

    y += titleLine + GapTitleToBody;

    const QFont &bodyFont = Font::tweakDesc();
    const qreal bodyLine = Css::normalLine(bodyFont);
    for (const QString &line : std::as_const(m_bodyLines)) {
        if (!line.isEmpty())
            Css::drawText(&p, inner, Css::baseline(bodyFont, y, bodyLine), bodyFont,
                          Color::TextDesc(), line);
        y += bodyLine;
    }

    // The panel behind the script, so the monospace block reads as inset rather than as
    // more body text in a different face.
    if (m_detailOpen && m_detailView) {
        const QRectF panel = QRectF(m_detailView->geometry())
                                 .adjusted(-DetailPad, -DetailPad, DetailPad, DetailPad);
        p.setPen(QPen(Color::Divider(), 1.0));
        p.setBrush(Color::Surface());
        p.drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5),
                          Metric::ControlRadius, Metric::ControlRadius);
    }
}

void Dialog::keyPressEvent(QKeyEvent *e)
{
    // Return accepts and Escape declines, which is what every other dialog on the machine
    // does. QDialog gives Escape for free; Return it only gives to a default QPushButton,
    // and there is no QPushButton here.
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        accept();
        return;
    }
    QDialog::keyPressEvent(e);
}

void Dialog::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        QDialog::mousePressEvent(e);
        return;
    }
    // A click on the scrim declines — the same gesture as clicking away from a popup.
    // Only for a two-answer card: dismissing a statement by missing the card would leave
    // the user unsure whether it had been read or lost.
    if (!cardRect().contains(e->position())) {
        if (m_reject)
            reject();
        return;
    }
    m_dragging = true;
    m_dragFrom = e->globalPosition().toPoint() - frameGeometry().topLeft();
}

void Dialog::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton))
        move(e->globalPosition().toPoint() - m_dragFrom);
}

void Dialog::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

// ---------------------------------------------------------------------------
// The two the call sites use
// ---------------------------------------------------------------------------

bool Dialog::confirm(QWidget *parent, const QString &title, const QString &body,
                     const QString &acceptText, const QString &rejectText,
                     const QString &detail)
{
    Dialog box(parent, title, body, acceptText, rejectText, detail);
    return box.exec() == QDialog::Accepted;
}

void Dialog::inform(QWidget *parent, const QString &title, const QString &body,
                    const QString &closeText, const QString &detail)
{
    Dialog box(parent, title, body, closeText, QString(), detail);
    box.exec();
}
