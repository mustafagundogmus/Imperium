#include "settingspage.h"

#include "../appstate.h"
#include "../catalog.h"
#include "../i18n.h"
#include "../preset.h"
#include "../settings.h"
#include "../theme.h"
#include "../updater.h"
#include "../winpaths.h"
#include "../widgets/accentpicker.h"
#include "../widgets/buttons.h"
#include "../widgets/languagepicker.h"
#include "../widgets/sectionheader.h"
#include "../widgets/segmentedcontrol.h"
#include "../widgets/themeswitch.h"
#include "../widgets/typefacepicker.h"
#include "../widgets/settingrow.h"
#include "../widgets/toggleswitch.h"

#include <QCoreApplication>
#include <QDesktopServices>

#include <iterator>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include "../widgets/dialog.h"
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

namespace {

/// A section is a header plus 1px-separated rows, exactly like a tweak section.
QWidget *makeSection(const QString &title, const QVector<QWidget *> &rows, QWidget *parent)
{
    auto *block = new QWidget(parent);
    auto *layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new SectionHeader(title, block);
    header->setCount(Locale::tr(QStringLiteral("tweak.sectionCount")).arg(rows.size()));
    layout->addWidget(header);

    auto *list = new QWidget(block);
    auto *listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(1);
    for (QWidget *row : rows) {
        row->setParent(list);
        listLayout->addWidget(row);
    }
    layout->addWidget(list);
    return block;
}

/// Two pill buttons side by side, used as one row's trailing control.
QWidget *makePair(PillButton *a, PillButton *b)
{
    auto *box = new QWidget;
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(a);
    layout->addWidget(b);
    box->adjustSize();
    return box;
}

} // namespace

SettingsPage::SettingsPage(AppState *state, Updater *updater, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_updater(updater)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                                 Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    m_layout->setSpacing(Theme::Metric::SectionGap);
    m_layout->addStretch(1);

    rebuild();

    connect(m_updater, &Updater::finished, this,
            [this](bool available, const QString &version, const QString &, const QString &error,
                   bool userInitiated) {
                m_updateButton->setEnabledLook(true);
                if (!error.isEmpty()) {
                    // A check nobody asked for should not report its own failure: the
                    // network being down at launch is not news the user requested.
                    if (!userInitiated)
                        return;
                    m_updateRow->setDesc(Locale::tr(QStringLiteral("settings.update.checkFailed")).arg(error));
                    Q_EMIT notice(Locale::tr(QStringLiteral("settings.update.checkFailedNotice")));
                    return;
                }
                if (available) {
                    // The row and the status line, and that is all this page does with the
                    // answer now. It used to open a browser here for a check somebody had
                    // pressed a button for, and say nothing but "press Denetle" for the
                    // launch-time one; MainWindow now puts the same offer up for both, and
                    // two things reacting to one signal would mean two things on screen.
                    // Which is also why the row no longer has a quiet wording: there is no
                    // longer a quiet outcome to describe.
                    m_updateRow->setDesc(Locale::tr(QStringLiteral("settings.update.available"))
                                             .arg(version));
                    Q_EMIT notice(Locale::tr(QStringLiteral("settings.update.newVersionNotice")).arg(version));
                } else if (userInitiated) {
                    m_updateRow->setDesc(Locale::tr(QStringLiteral("settings.update.upToDate"))
                                             .arg(QCoreApplication::applicationVersion()));
                    Q_EMIT notice(Locale::tr(QStringLiteral("settings.update.upToDateNotice")));
                }
            });

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &SettingsPage::rebuild);
}

void SettingsPage::rebuild()
{
    // Everything from the stretch backward. The language row's own click is what gets
    // here — its LanguagePicker is one of the widgets this loop is about to tear down —
    // so the widgets are only hidden and unparented now, with the actual delete deferred
    // to the next spin of the event loop. Deleting synchronously would destroy that
    // picker while it is still unwinding its own mouseReleaseEvent, which is a
    // use-after-free the moment control returns to it.
    while (m_layout->count() > 1) {
        QLayoutItem *item = m_layout->takeAt(0);
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }

    m_rowCount = 0;
    m_layout->insertWidget(0, buildAppearance());
    m_layout->insertWidget(1, buildInterface());
    m_layout->insertWidget(2, buildPresets());
    m_layout->insertWidget(3, buildApplication());
    m_layout->insertWidget(4, buildSafety());

    // The restore-point probe runs once at startup; a rebuild would otherwise show
    // "Aranıyor…" again for a result that already arrived.
    if (m_restoreKnown)
        setRestorePoint(m_restoreValue);
}

/// Görünüm — the five settings that are a choice from a set, each one a caption over the
/// gallery that makes the choice.
///
/// Dil comes first. It is the one setting on this page that rewrites every other word on
/// it, including the labels of everything below, so somebody who has landed in a language
/// they cannot read has to be able to find it without reading anything — and the ten
/// native names are legible whichever language is in force. It stays in this section
/// rather than getting one of its own because that is where the rest of the app already
/// files it: the Hakkında page's Görünüm card counts the languages alongside the themes,
/// the accents and the faces.
QWidget *SettingsPage::buildAppearance()
{
    auto *themePicker = new ThemeSwitch;
    connect(themePicker, &ThemeSwitch::picked, this, [](Theme::Appearance a) {
        Theme::setAppearance(a);
    });

    auto *typeface = new TypefacePicker;
    connect(typeface, &TypefacePicker::picked, this, [](const QString &id) {
        Theme::setTypeface(id);
    });

    auto *language = new LanguagePicker;
    connect(language, &LanguagePicker::picked, this, [](const QString &id) {
        Locale::setLanguage(id);
    });

    auto *accent = new AccentPicker;
    connect(accent, &AccentPicker::picked, this, [](const QColor &c) { Theme::setAccent(c); });

    // The shell: the one choice here that changes the shape of the window around this
    // page rather than a detail inside it. Second, right under the language, so it is on
    // the first screen of the page rather than filed under the switches further down —
    // which is where it was first put, and where nobody found it.
    auto *shell = new SegmentedControl({Locale::tr(QStringLiteral("settings.shell.classic")),
                                        Locale::tr(QStringLiteral("settings.shell.fluent"))});
    shell->setCurrentIndex(Theme::fluent() ? 1 : 0);
    connect(shell, &SegmentedControl::currentIndexChanged, this, [](int i) {
        Theme::setShell(i == 1 ? Theme::Shell::Fluent : Theme::Shell::Classic);
    });

    static const char *const ScaleKeys[] = {
        "settings.fontsize.small", "settings.fontsize.normal",
        "settings.fontsize.large", "settings.fontsize.xlarge",
    };
    QStringList scaleLabels;
    int scaleCurrent = 1;   // default to "Normal" when nothing matches
    for (int i = 0; i < int(std::size(Theme::FontScaleSteps)); ++i) {
        scaleLabels << Locale::tr(QString::fromLatin1(ScaleKeys[i]));
        if (qFuzzyCompare(Theme::FontScaleSteps[i].value, Theme::fontScale()))
            scaleCurrent = i;
    }
    auto *fontSize = new SegmentedControl(scaleLabels);
    fontSize->setCurrentIndex(scaleCurrent);
    connect(fontSize, &SegmentedControl::currentIndexChanged, this, [](int i) {
        Theme::setFontScale(Theme::FontScaleSteps[i].value);
    });

    // Every one of the five is Below: the caption gets the whole column and so does the
    // gallery under it. They used to be trailing controls, which is what made this section
    // unreadable — the eight theme cards took 372px out of the right of a row and pushed
    // it to 160px tall, the ten language chips took another 400 and wrapped into a ragged
    // block, and the two descriptions that had the most to say were left with the least
    // room to say it in.
    const QVector<QWidget *> rows{
        new SettingRow(Locale::tr(QStringLiteral("settings.language.label")),
                       Locale::tr(QStringLiteral("settings.language.desc")),
                       language, SettingRow::Below),
        new SettingRow(Locale::tr(QStringLiteral("settings.shell.label")),
                       Locale::tr(QStringLiteral("settings.shell.desc")),
                       shell, SettingRow::Below),
        new SettingRow(Locale::tr(QStringLiteral("settings.theme.label")),
                       Locale::tr(QStringLiteral("settings.theme.desc")),
                       themePicker, SettingRow::Below),
        new SettingRow(Locale::tr(QStringLiteral("settings.accent.label")),
                       Locale::tr(QStringLiteral("settings.accent.desc")),
                       accent, SettingRow::Below),
        new SettingRow(Locale::tr(QStringLiteral("settings.typeface.label")),
                       Locale::tr(QStringLiteral("settings.typeface.desc")),
                       typeface, SettingRow::Below),
        new SettingRow(Locale::tr(QStringLiteral("settings.fontsize.label")),
                       Locale::tr(QStringLiteral("settings.fontsize.desc")),
                       fontSize, SettingRow::Below),
    };
    m_rowCount += rows.size();
    return makeSection(Locale::tr(QStringLiteral("settings.section.appearance")), rows, this);
}

/// Arayüz — the three switches that tune how the interface behaves rather than what it is
/// drawn in. They were the tail of Görünüm, where they read as three more of the same kind
/// of thing as the theme and the language and are not: a switch has no options to show, it
/// is on or off. Splitting them leaves Görünüm as five galleries and nothing else, which
/// is what lets one section have one shape.
QWidget *SettingsPage::buildInterface()
{
    auto *compact = new ToggleSwitch;
    compact->setChecked(Theme::compact(), false);
    connect(compact, &ToggleSwitch::toggled, this, [](bool on) { Theme::setCompact(on); });

    auto *smooth = new ToggleSwitch;
    smooth->setChecked(Settings::instance().smoothScroll(), false);
    connect(smooth, &ToggleSwitch::toggled, this, [](bool on) { Settings::instance().setSmoothScroll(on); });

    auto *glow = new ToggleSwitch;
    glow->setChecked(Settings::instance().borderGlow(), false);
    connect(glow, &ToggleSwitch::toggled, this, [](bool on) { Settings::instance().setBorderGlow(on); });

    const QVector<QWidget *> rows{
        new SettingRow(Locale::tr(QStringLiteral("settings.compact.label")),
                       Locale::tr(QStringLiteral("settings.compact.desc")),
                       compact, SettingRow::Leading),
        new SettingRow(Locale::tr(QStringLiteral("settings.smoothscroll.label")),
                       Locale::tr(QStringLiteral("settings.smoothscroll.desc")),
                       smooth, SettingRow::Leading),
        new SettingRow(Locale::tr(QStringLiteral("settings.glow.label")),
                       Locale::tr(QStringLiteral("settings.glow.desc")),
                       glow, SettingRow::Leading),
    };
    m_rowCount += rows.size();
    return makeSection(Locale::tr(QStringLiteral("settings.section.interface")), rows, this);
}

QWidget *SettingsPage::buildPresets()
{
    auto *save = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.preset.export")));
    auto *load = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.preset.import")));
    connect(save, &PillButton::clicked, this, &SettingsPage::onExportPreset);
    connect(load, &PillButton::clicked, this, &SettingsPage::onImportPreset);

    auto *openFolder = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.presetFolder.open")));
    connect(openFolder, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Preset::directory()));
    });

    auto *regExport = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.preset.export")));
    connect(regExport, &PillButton::clicked, this, &SettingsPage::onExportReg);

    m_presetRow = new SettingRow(
        Locale::tr(QStringLiteral("settings.preset.label")),
        Locale::tr(QStringLiteral("settings.preset.desc")),
        makePair(save, load), SettingRow::Trailing);

    const QVector<QWidget *> rows{
        m_presetRow,
        new SettingRow(Locale::tr(QStringLiteral("settings.reg.label")),
                       Locale::tr(QStringLiteral("settings.reg.desc")),
                       regExport, SettingRow::Trailing),
        new SettingRow(Locale::tr(QStringLiteral("settings.presetFolder.label")),
                       QDir::toNativeSeparators(Preset::directory()),
                       openFolder, SettingRow::Trailing),
    };
    m_rowCount += rows.size();
    return makeSection(Locale::tr(QStringLiteral("settings.section.presets")), rows, this);
}

QWidget *SettingsPage::buildApplication()
{
    m_updateButton = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.update.check")));
    connect(m_updateButton, &PillButton::clicked, this, &SettingsPage::onCheckUpdates);

    m_updateRow = new SettingRow(
        Locale::tr(QStringLiteral("settings.update.label")),
        Locale::tr(QStringLiteral("settings.update.desc")).arg(Updater::repository() + QStringLiteral(" (Imperium.exe)")),
        m_updateButton, SettingRow::Trailing);

    auto *onLaunch = new ToggleSwitch;
    onLaunch->setChecked(Settings::instance().checkUpdatesOnLaunch(), false);
    connect(onLaunch, &ToggleSwitch::toggled, this,
            [](bool on) { Settings::instance().setCheckUpdatesOnLaunch(on); });

    auto *openRepo = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.repo.open")));
    connect(openRepo, &PillButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(Updater::releasesUrl())); });

    const QVector<QWidget *> rows{
        m_updateRow,
        // A new description rather than the old one. The old text promised the launch
        // check looked "silently", which was the whole of what it did; it now ends in an
        // offer to replace the executable, and it is on by default, so this row is the
        // one place a user finds out both facts and the one switch that turns them off.
        new SettingRow(Locale::tr(QStringLiteral("settings.update.onLaunch.label")),
                       Locale::tr(QStringLiteral("update.onLaunch.desc")),
                       onLaunch, SettingRow::Leading),
        new SettingRow(Locale::tr(QStringLiteral("settings.version.label")),
                       Locale::tr(QStringLiteral("settings.version.value"))
                           .arg(QCoreApplication::applicationVersion(), QT_VERSION_STR)
                           .arg(Catalog::instance().totalTweaks()),
                       openRepo, SettingRow::Trailing),
    };
    m_rowCount += rows.size();
    return makeSection(Locale::tr(QStringLiteral("settings.section.application")), rows, this);
}

QWidget *SettingsPage::buildSafety()
{
    auto *confirm = new ToggleSwitch;
    confirm->setChecked(Settings::instance().confirmBeforeApply(), false);
    connect(confirm, &ToggleSwitch::toggled, this,
            [](bool on) { Settings::instance().setConfirmBeforeApply(on); });

    auto *openJournal = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.journalRow.open")));
    connect(openJournal, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    });

    auto *restorePoint = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("settings.restore.button")));
    connect(restorePoint, &PillButton::clicked, this, [] {
        // Creating a restore point changes the system, which this build deliberately does
        // not do — so hand the user Windows' own System Protection dialog instead.
        // From System32 by absolute path, like every other program this elevated process
        // starts; a bare name would be resolved through the inherited PATH.
        const QString protection =
            WinPaths::system32() + QStringLiteral("\\SystemPropertiesProtection.exe");
        if (!QFileInfo::exists(protection) || !QProcess::startDetached(protection, {}))
            QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:about")));
    });

    m_restoreRow = new SettingRow(
        Locale::tr(QStringLiteral("settings.restore.label")),
        Locale::tr(QStringLiteral("settings.restore.searching")),
        restorePoint, SettingRow::Trailing);

    QVector<QWidget *> rows{
        new SettingRow(Locale::tr(QStringLiteral("settings.confirm.label")),
                       Locale::tr(QStringLiteral("settings.confirm.desc")),
                       confirm, SettingRow::Leading),
        new SettingRow(Locale::tr(QStringLiteral("settings.journalRow.label")),
                       Locale::tr(QStringLiteral("settings.journalRow.desc")),
                       openJournal, SettingRow::Trailing),
        m_restoreRow,
    };

    // No "run as administrator" row: the manifest asks for elevation at launch, so by the
    // time this page is on screen the app already has the token, and a row offering to
    // get what it has would be furniture.

    m_rowCount += rows.size();
    return makeSection(Locale::tr(QStringLiteral("settings.section.safety")), rows, this);
}

void SettingsPage::setRestorePoint(const QString &value)
{
    m_restoreValue = value;
    m_restoreKnown = true;
    if (!m_restoreRow)
        return;
    m_restoreRow->setDesc(Locale::tr(QStringLiteral("settings.restore.point"))
                              .arg(value.isEmpty() ? Locale::tr(QStringLiteral("settings.restore.none"))
                                                   : value.toLower()));
}

void SettingsPage::onExportPreset()
{
    const QString suggested = QDir(Preset::directory())
                                  .filePath(Preset::fileNameFor(QStringLiteral("onayar")));
    const QString path = QFileDialog::getSaveFileName(
        this, Locale::tr(QStringLiteral("settings.preset.exportTitle")), suggested,
        Locale::tr(QStringLiteral("settings.preset.filter")));
    if (path.isEmpty())
        return;

    // What the user changed, not the whole catalogue. This used to insert every id the
    // catalogue holds — 706 of them on the machine this was measured on: the catalogued
    // tweaks plus one row per installed service and one per startup entry, both
    // synthesised from the live machine — so a file the user thought of as "my settings"
    // was seven hundred lines and carried machine A's entire service configuration onto
    // machine B.
    //
    // The predicate is isOn(): the tweak is not sitting where Windows ships it. That is
    // the same question the "Etkin" filter and the overview's count already ask, so what
    // lands in the file is what the app has been calling changed all along. On the same
    // machine it selects 99 of the 706.
    //
    // Deliberately isOn() rather than isPending(). Pending is selected != *applied*, and
    // pressing Uygula empties the pending set by definition — measured on that machine it
    // is zero the moment the app has finished reading the registry. A preset exported the
    // way a preset is actually exported, from a machine the user has just finished setting
    // up and applying, would have contained nothing at all. isPending() is still OR'd in
    // for the other direction: a tweak queued back to the position Windows ships is a
    // deliberate choice that has not been applied yet, and it belongs in the file at the
    // position the user picked.
    //
    // What this leaves out is the part worth being careful about, because it is the whole
    // reason the file is a hundred lines instead of seven hundred. A service's
    // defaultOption is not a constant: catalog.cpp's appendServices() derives it from the
    // Start value actually found on this machine — "there is no universal default for a
    // service" — so isOn() is false for every service the user has not moved himself, and
    // all 307 of them drop out on their own. Measured: 0 exported.
    //
    // Startup entries do not work that way and the difference is deliberate here too.
    // appendStartup() sets a fixed defaultOption of 1, because Windows does run a startup
    // entry unless something says otherwise, so an entry that is disabled on this machine
    // travels whoever disabled it — 8 of 9 on the machine above. That is the honest
    // answer rather than an accident: every other count in the app calls a disabled
    // startup entry changed, and a second machine set up from this file is meant to end up
    // with the same programs not launching.
    //
    // (`literal` is a third thing and decides none of this. It tells the engine to write
    // the position's own bytes instead of consulting the journal; both kinds of
    // synthesised row set it, for two different reasons of their own.)
    QHash<QString, int> positions;
    forEachTweak(Catalog::instance(), [&](const Tweak &t) {
        if (m_state->isOn(t.id) || m_state->isPending(t.id))
            positions.insert(t.id, m_state->selected(t.id));
    });

    QString error;
    const QString name = QFileInfo(path).completeBaseName();
    if (Preset::save(path, name, positions, Preset::Scope::Changed,
                     Preset::currentAppearance(), Preset::currentSettings(), &error))
        Q_EMIT notice(Locale::tr(QStringLiteral("settings.preset.saved")).arg(positions.size()));
    else
        Q_EMIT notice(Locale::tr(QStringLiteral("settings.preset.saveFailed")).arg(error));
}

void SettingsPage::onImportPreset()
{
    const QString path = QFileDialog::getOpenFileName(
        this, Locale::tr(QStringLiteral("settings.preset.importTitle")), Preset::directory(),
        Locale::tr(QStringLiteral("settings.preset.filter")));
    if (path.isEmpty())
        return;

    const Preset::LoadResult result = Preset::load(path);
    if (!result.ok) {
        Q_EMIT notice(Locale::tr(QStringLiteral("settings.preset.readFailed")).arg(result.error));
        return;
    }

    // A preset only moves controls; nothing is written until the user presses Uygula.
    int changed = 0;
    for (auto it = result.positions.cbegin(); it != result.positions.cend(); ++it) {
        if (m_state->selected(it.key()) == it.value())
            continue;
        m_state->setSelected(it.key(), it.value());
        ++changed;
    }

    // The appearance and the four switches cannot be queued behind Uygula the way the
    // positions above are, because there is nothing for Uygula to write: they are the app's
    // own preferences, not machine writes, so they take effect the moment they are applied.
    // Which is precisely why the user is asked first — repainting somebody's window and
    // switching its language because they opened a file is its own kind of surprise. Asked
    // once, for both elements together, since answering "yes, make this machine look like
    // that one" twice is not a finer-grained choice, only a longer one. The tweak positions
    // are queued either way; this dialog can only decline the preferences.
    if (result.appearance.present || result.settings.present) {
        const bool adopt = Dialog::confirm(
            this,
            Locale::tr(QStringLiteral("settings.preset.prefs.title")),
            Locale::tr(QStringLiteral("settings.preset.prefs.body")),
            Locale::tr(QStringLiteral("settings.preset.prefs.apply")),
            Locale::tr(QStringLiteral("settings.preset.prefs.keep")));
        if (adopt) {
            // The four switches first. applyAppearance() finishes with the language, and
            // a language change tears this page down and builds it again through
            // rebuild() — which reads Settings::instance() to decide where the smooth
            // scroll and border glow toggles sit. Applied the other way round, the rebuilt
            // rows would be drawn from the values the preset was about to replace.
            Preset::applySettings(result.settings);
            Preset::applyAppearance(result.appearance);

            // And a rebuild of our own, because a preset that moves the density, the text
            // size or any of the four switches without moving the language changes nothing
            // on screen otherwise. Those five controls are read out of Theme and Settings
            // once, when the row is built, and have no signal to follow afterwards —
            // unlike the theme, accent, typeface and language pickers, which repaint
            // themselves off Theme::notifier() and were the only reason this looked like
            // it worked. Calling it from inside the button handler that got us here is
            // safe for the reason rebuild() itself gives: it hides and unparents now and
            // defers every delete to the next spin of the event loop. When the language
            // did change this is the second rebuild rather than the first, which costs one
            // wasted pass over four sections and keeps the rule simple.
            rebuild();
        }
    }

    // Emitted last so the notice the window writes is in the language the preset just
    // asked for, rather than the one the user was reading a moment ago.
    Q_EMIT presetApplied(result.meta.name.isEmpty() ? QFileInfo(path).completeBaseName()
                                                    : result.meta.name,
                         changed, result.unknownIds);
}

void SettingsPage::onExportReg()
{
    const QList<QString> pending = m_state->pendingIds();
    if (pending.isEmpty()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("settings.reg.none")));
        return;
    }

    const QString suggested = QDir(Preset::directory())
                                  .filePath(QStringLiteral("arbitrium-degisiklikler.reg"));
    const QString path = QFileDialog::getSaveFileName(
        this, Locale::tr(QStringLiteral("settings.reg.exportTitle")), suggested,
        Locale::tr(QStringLiteral("settings.reg.filter")));
    if (path.isEmpty())
        return;

    QHash<QString, int> positions;
    QStringList ids;
    for (const QString &id : pending) {
        ids << id;
        positions.insert(id, m_state->selected(id));
    }
    ids.sort();

    QString error;
    const int written = Preset::exportRegFile(path, ids, positions, &error);
    if (written < 0)
        Q_EMIT notice(Locale::tr(QStringLiteral("settings.reg.writeFailed")).arg(error));
    else
        Q_EMIT notice(Locale::tr(QStringLiteral("settings.reg.written"))
                          .arg(ids.size()).arg(written));
}

void SettingsPage::onCheckUpdates()
{
    m_updateButton->setEnabledLook(false);
    m_updateRow->setDesc(Locale::tr(QStringLiteral("settings.update.checking")));
    m_updater->check(true);
}
