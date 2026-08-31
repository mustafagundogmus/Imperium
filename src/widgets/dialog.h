// dialog.h — the application's own message box.
//
// Every other surface in this app is drawn by it: the same palette, the same type scale,
// the same 1px rules, the same two buttons. QMessageBox was the one place that was not.
// It brought Windows' own window frame, Windows' own title bar, its own grey, its own
// button metrics and its own font — in a frameless, themed, ten-language application it
// read as a different program interrupting this one. It also could not follow the theme:
// on the light palettes it stayed whatever the system said, which is the same class of
// fault the palettes above were measured to avoid.
//
// So this is that box, in the design's own vocabulary:
//
//   card       Tile on a scrim of Window, TileBorder hairline, ControlRadius corners —
//              the ApplyOverlay card, because a modal question and a modal progress
//              report should not be two different objects.
//   heading    Font::blockTitle(), the weight the Genel Bakış cards use for their titles
//   body       Font::tweakDesc(), wrapped rather than elided; a dialog is the one place
//              the text has to be read in full
//   detail     an inset panel of Font::infoValueMono() behind a "Ayrıntılar" toggle, for
//              the action scripts — selectable and copyable, which QMessageBox's own
//              detail box never was
//   buttons    PillButton::Accent to accept, PillButton::Ghost to decline
//
// Modal the way QMessageBox was: confirm() and inform() block on their own event loop and
// return once the user has answered, so the call sites keep the shape they already had.
//
// Escape and a click on the scrim both decline. Return accepts. The card is draggable by
// any part of itself that is not a control, like the window it sits over.

#pragma once

#include <QDialog>
#include <QPoint>
#include <QString>
#include <QStringList>

class PillButton;
class QPlainTextEdit;

class Dialog : public QDialog
{
    Q_OBJECT

public:
    /// A question with two answers. Returns true when \a acceptText was chosen.
    ///
    /// \a detail, when given, is the exact text the question is about — an action's whole
    /// script — put behind a toggle rather than in the body, because it is evidence for
    /// the user who wants it and noise for the one who does not.
    static bool confirm(QWidget *parent, const QString &title, const QString &body,
                        const QString &acceptText, const QString &rejectText,
                        const QString &detail = QString());

    /// A statement with one button. Returns when it is dismissed.
    static void inform(QWidget *parent, const QString &title, const QString &body,
                       const QString &closeText, const QString &detail = QString());

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    Dialog(QWidget *parent, const QString &title, const QString &body,
           const QString &acceptText, const QString &rejectText, const QString &detail);

    /// Wraps the body at the card's text width and sizes the window to what that needs.
    /// Called again when the detail panel opens or closes, which is the only thing that
    /// changes the height after construction.
    void relayout();

    /// Centred on the window that owns it rather than on the screen: on a second monitor
    /// a screen-centred dialog appears somewhere the user is not looking.
    void centreOnParent();

    /// The card, in widget coordinates. Everything outside it is scrim.
    QRectF cardRect() const;

    qreal m_cardHeight = 0.0;   ///< measured by relayout(), read by cardRect()

    QString m_title;
    QString m_body;
    QString m_detail;
    QStringList m_bodyLines;   ///< m_body wrapped, cached by relayout()

    PillButton *m_accept = nullptr;
    PillButton *m_reject = nullptr;
    PillButton *m_toggle = nullptr;   ///< "Ayrıntılar", only when m_detail is set
    QPlainTextEdit *m_detailView = nullptr;

    bool m_detailOpen = false;
    bool m_dragging = false;
    QPoint m_dragFrom;
};
