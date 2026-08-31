// updatedialog.h — what "update itself" looks like while it is happening.
//
// Two things, in order. First the offer: a Dialog::confirm built the way
// ActionPage::confirmAndRun builds its confirmation — the new version as the title, what
// is about to happen as the body, the release notes behind the Ayrıntılar toggle, and two
// plain buttons. The app has one way of asking before it does something irreversible and
// this is it, so the update asks the same way.
//
// Then, only if the user accepted, this dialog: the progress card, drawn with the same
// parts as the apply overlay — the uppercase stage label, the 4px accent rail, the name
// and the meta line under it — so the two read as the same program doing two kinds of
// work.
//
// A dialog rather than a second ApplyOverlay, which is the other thing it could have
// been. The overlay is a hand-placed child of MainWindow's card whose geometry MainWindow
// keeps in step through resizeEvent and changeEvent, whose Escape has to be negotiated
// with a window-level shortcut, and whose contract is "a registry write is in flight, the
// content area is off limits, the title bar stays reachable". None of that fits an update:
// it must stay in front while the window behind it is being torn down at exit, it is
// started from a modal offer and is the same conversation continuing, and it ends by
// terminating the process — so its own lifetime is the natural place to hold the whole
// flow, and a top-level modal window gives that without adding a second geometry to
// MainWindow or a second shortcut to negotiate. It borrows the overlay's look, not its
// plumbing.

#pragma once

#include "../updater.h"

#include <QDialog>

class PillButton;

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    /// Offers \a release to the user and, if they accept, runs the whole update: two
    /// downloads, the digest comparison, the swap and the restart. Returns as soon as the
    /// offer has been answered; everything after that is driven by \a updater's signals.
    ///
    /// Refusals that can be known before anything is downloaded are answered here rather
    /// than half way through: a release that published no Windows executable, and a folder
    /// this process cannot write into.
    static void offer(Updater *updater, const Updater::Release &release, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *) override;
    void reject() override;   ///< Escape and the window's close button both mean cancel

private:
    UpdateDialog(Updater *updater, const Updater::Release &release, QWidget *parent);

    void setStage(Updater::Stage stage);

    Updater *m_updater = nullptr;
    Updater::Release m_release;

    QString m_heading;   ///< uppercase stage label, top left
    QString m_counter;   ///< "%42", top right
    QString m_meta;      ///< byte counts, then what the digest check does and does not say
    qreal m_fraction = 0.0;

    PillButton *m_cancel = nullptr;
};
