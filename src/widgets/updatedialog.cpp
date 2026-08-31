#include "updatedialog.h"

#include "buttons.h"
#include "../css.h"
#include "../i18n.h"
#include "../theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
// QMessageBox::addButton hands back a QPushButton*, so the type has to be complete for
// the QAbstractButton* the comparison below is written against — the same pair
// settingspage.cpp and mainwindow.cpp carry for their own dialogs.
#include <QPushButton>

#include <cmath>

namespace {

constexpr int CardWidth = 520;
constexpr qreal Pad = 18.0;
constexpr qreal RailHeight = 4.0;
constexpr qreal GapAboveRail = 12.0;
constexpr qreal GapBelowRail = 12.0;
constexpr qreal GapNameToMeta = 3.0;
constexpr qreal GapToButton = 16.0;

/// One-button dialog for the branches that end the update before it starts, or after it
/// has already put everything back. Same construction as every other message this app
/// puts up: no icon, the app's name in the title bar, the buttons labelled from the
/// translation table rather than by Qt.
void say(QWidget *parent, const QString &text, const QString &informative,
         const QString &details)
{
    QMessageBox box(parent);
    box.setWindowTitle(QCoreApplication::applicationName());
    box.setIcon(QMessageBox::NoIcon);
    box.setText(text);
    box.setInformativeText(informative);
    if (!details.isEmpty())
        box.setDetailedText(details);
    box.addButton(Locale::tr(QStringLiteral("apply.close")), QMessageBox::AcceptRole);
    box.exec();
}

} // namespace

void UpdateDialog::offer(Updater *updater, const Updater::Release &release, QWidget *parent)
{
    const QString title = Locale::tr(QStringLiteral("update.offer.title")).arg(release.version);

    // The release page always goes into the details, whether or not there are notes, so
    // there is a way to the release from every one of these dialogs — including the two
    // that refuse. Without it a user told "this folder is read-only" would have nowhere
    // to go next.
    QString details = release.notes;
    if (!details.isEmpty())
        details += QStringLiteral("\n\n");
    details += release.pageUrl;

    // A release that carries no Windows executable, or whose assets are not published
    // under the pinned repository, cannot be installed by this program. Said now, from
    // the names alone, rather than after a download.
    if (!release.installable()) {
        say(parent, title, Locale::tr(QStringLiteral("update.fail.asset")), details);
        return;
    }

    // And the folder, before anything is fetched. This is the failure worth catching
    // early: everything else costs a round trip to discover, this one would cost the
    // whole download and then strand the user at the last step.
    QString reason;
    if (!Updater::installFolderWritable(&reason)) {
        say(parent, title,
            Locale::tr(QStringLiteral("update.fail.folder"))
                .arg(QDir::toNativeSeparators(Updater::installFolder()), reason),
            details);
        return;
    }

    QMessageBox box(parent);
    box.setWindowTitle(QCoreApplication::applicationName());
    box.setIcon(QMessageBox::NoIcon);
    box.setText(title);
    // What is about to happen, then what the verification proves and what it does not.
    // The honest limit of a checksum belongs here, before the user agrees, rather than
    // next to a checkmark afterwards where it would read as reassurance.
    box.setInformativeText(Locale::tr(QStringLiteral("update.offer.body"))
                               .arg(QCoreApplication::applicationVersion())
                           + QStringLiteral("\n\n")
                           + Locale::tr(QStringLiteral("update.proof")));
    // Not a formality, exactly as in ActionPage::confirmAndRun: the whole release note is
    // here, so a user who wants to know what changed can read it instead of taking the
    // version number on trust.
    box.setDetailedText(details);

    QPushButton *go = box.addButton(Locale::tr(QStringLiteral("update.offer.go")),
                                    QMessageBox::AcceptRole);
    // "Şimdilik kalsın" — the same words the elevation dialog already uses to mean "not
    // now, and do not do anything".
    box.addButton(Locale::tr(QStringLiteral("mw.elevate.later")), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() != go)
        return;

    // Parented so it centres on the window and dies with it; not WA_DeleteOnClose,
    // because both of the handlers below outlive the close by one nested event loop.
    auto *dialog = new UpdateDialog(updater, release, parent);
    dialog->show();
    updater->install(release);
}

UpdateDialog::UpdateDialog(Updater *updater, const Updater::Release &release, QWidget *parent)
    : QDialog(parent)
    , m_updater(updater)
    , m_release(release)
{
    setWindowTitle(QCoreApplication::applicationName());
    // Application modal: the update is going to replace the executable this window is
    // running from, so nothing else in the app may be touched while it runs.
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    m_cancel = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("actions.cancel")), this);
    connect(m_cancel, &PillButton::clicked, this, &UpdateDialog::reject);

    setStage(Updater::Stage::Download);

    using namespace Theme;
    const qreal headingLine = Css::normalLine(Font::sectionTitle());
    const qreal nameLine = Css::normalLine(Font::tweakName());
    const qreal metaLine = Css::normalLine(Font::tweakDesc());
    const int height = int(std::round(2 * Pad + headingLine + GapAboveRail + RailHeight
                                      + GapBelowRail + nameLine + GapNameToMeta + metaLine
                                      + GapToButton))
                       + m_cancel->sizeHint().height();
    setFixedSize(CardWidth, height);
    m_cancel->move(CardWidth - int(Pad) - m_cancel->sizeHint().width(),
                   height - int(Pad) - m_cancel->sizeHint().height());

    connect(m_updater, &Updater::stageChanged, this, &UpdateDialog::setStage);

    connect(m_updater, &Updater::progress, this, [this](qint64 received, qint64 total) {
        // A server that answers without a Content-Length gives -1 here. The rail then has
        // nothing honest to show, so it stays empty and the meta line counts up alone
        // rather than inventing a proportion.
        if (total > 0) {
            m_fraction = qBound(0.0, qreal(received) / qreal(total), 1.0);
            m_counter = QStringLiteral("%%1").arg(int(m_fraction * 100));
            m_meta = QStringLiteral("%1 / %2").arg(QLocale().formattedDataSize(received),
                                                   QLocale().formattedDataSize(total));
        } else {
            m_meta = QLocale().formattedDataSize(received);
        }
        update();
    });

    connect(m_updater, &Updater::installFailed, this, [this](const QString &message) {
        // Copied out before anything closes: the members go with the dialog and this
        // handler has to outlive it by one nested event loop.
        QWidget *host = parentWidget();
        const QString title = Locale::tr(QStringLiteral("update.offer.title")).arg(m_release.version);
        const QString page = m_release.pageUrl;
        hide();
        deleteLater();
        say(host, title, message, page);
    });

    connect(m_updater, &Updater::installReady, this, [this] {
        hide();
        deleteLater();
        // The new executable is already running. This process is the only thing still
        // holding the old binary open, so the useful thing it can do now is stop: main()
        // returns, the window is destroyed, and the next start sweeps the leftover away.
        QCoreApplication::quit();
    });

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this,
            qOverload<>(&QWidget::update));
}

void UpdateDialog::setStage(Updater::Stage stage)
{
    switch (stage) {
    case Updater::Stage::Download:
        m_heading = Locale::tr(QStringLiteral("update.stage.download"));
        break;
    case Updater::Stage::Verify:
        m_heading = Locale::tr(QStringLiteral("update.stage.verify"));
        // From here on the meta line carries the sentence about what the digest proves,
        // which is the same one the offer showed in full. Elided to the card, so the
        // tooltip keeps the whole of it reachable.
        m_meta = Locale::tr(QStringLiteral("update.proof"));
        setToolTip(m_meta);
        m_fraction = 1.0;
        m_counter.clear();
        break;
    case Updater::Stage::Install:
        m_heading = Locale::tr(QStringLiteral("update.stage.install"));
        m_fraction = 1.0;
        m_counter.clear();
        // Renaming two files and starting one takes milliseconds and cannot be undone
        // half way, so there is nothing left to cancel. The button says so by going dim
        // rather than disappearing and moving everything under it.
        if (m_cancel)
            m_cancel->setEnabledLook(false);
        break;
    }
    update();
}

void UpdateDialog::reject()
{
    if (m_updater->installing())
        m_updater->cancelInstall();
    QDialog::reject();
    // Nothing touches the dialog after this, and nothing is left on disk to tidy: until
    // the digest matches, everything the update has fetched is in memory.
    deleteLater();
}

void UpdateDialog::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // The dialog *is* the card — it carries the window manager's own frame around it, so
    // there is no scrim and no rounded tile floating on one, only the same ground and the
    // same 1px edge the overview tiles use.
    p.fillRect(rect(), Color::Tile());
    p.setPen(QPen(Color::TileBorder(), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

    const QRectF inner(Pad, 0, width() - 2 * Pad, height());

    // --- heading -----------------------------------------------------------
    const QFont &headingFont = Font::sectionTitle();
    const qreal headingLine = Css::normalLine(headingFont);
    qreal y = Pad;

    Css::drawText(&p, inner, Css::baseline(headingFont, y, headingLine), headingFont,
                  Color::TextDim(), Css::upperTr(m_heading));

    const QFont &counterFont = Font::sectionCount();
    Css::drawText(&p, inner, Css::baseline(counterFont, y, headingLine), counterFont,
                  Color::TextFainter(), m_counter, Qt::AlignRight);

    // --- rail --------------------------------------------------------------
    y += headingLine + GapAboveRail;
    const QRectF rail(inner.left(), y, inner.width(), RailHeight);
    const qreal radius = RailHeight / 2.0;

    QPainterPath railPath;
    railPath.addRoundedRect(rail, radius, radius);
    p.setPen(Qt::NoPen);
    p.setBrush(Color::ToggleOff());
    p.drawPath(railPath);

    const qreal filled = qBound(0.0, m_fraction, 1.0) * rail.width();
    if (filled > 0.5) {
        p.save();
        p.setClipPath(railPath);
        p.setBrush(Theme::accent());
        p.drawRect(QRectF(rail.left(), rail.top(), filled, rail.height()));
        p.restore();
    }

    // --- what is being fetched, and how far ---------------------------------
    y += RailHeight + GapBelowRail;
    const QFont &nameFont = Font::tweakName();
    const qreal nameLine = Css::normalLine(nameFont);
    Css::drawText(&p, inner, Css::baseline(nameFont, y, nameLine), nameFont,
                  Color::TextPrimary(), m_release.exeName, Qt::AlignLeft, true);

    y += nameLine + GapNameToMeta;
    const QFont &metaFont = Font::tweakDesc();
    const qreal metaLine = Css::normalLine(metaFont);
    Css::drawText(&p, inner, Css::baseline(metaFont, y, metaLine), metaFont,
                  Color::TextFaint(), m_meta, Qt::AlignLeft, true);
}
