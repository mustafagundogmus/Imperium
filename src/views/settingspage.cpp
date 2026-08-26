#include "settingspage.h"

#include "../appstate.h"
#include "../catalog.h"
#include "../i18n.h"
#include "../preset.h"
#include "../settings.h"
#include "../theme.h"
#include "../updater.h"
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
            [this](bool available, const QString &version, const QString &url, const QString &error,
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
                    // Only a check somebody pressed a button for is allowed to take over
                    // the screen. The launch-time one says so and stops there — its own
                    // setting promises it looks "silently", and a browser opening by
                    // itself seconds after the window appears is the opposite of that.
                    m_updateRow->setDesc(Locale::tr(userInitiated
                                                        ? QStringLiteral("settings.update.available")
                                                        : QStringLiteral("settings.update.availableQuiet"))
                                             .arg(version));
                    Q_EMIT notice(Locale::tr(QStringLiteral("settings.update.newVersionNotice")).arg(version));
                    if (userInitiated)
                        QDesktopServices::openUrl(QUrl(url));
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
    m_layout->insertWidget(1, buildPresets());
    m_layout->insertWidget(2, buildApplication());
    m_layout->insertWidget(3, buildSafety());

    // The restore-point probe runs once at startup; a rebuild would otherwise show
    // "Aranıyor…" again for a result that already arrived.
    if (m_restoreKnown)
        setRestorePoint(m_restoreValue);
}

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
        new SettingRow(Locale::tr(QStringLiteral("settings.theme.label")),
                       Locale::tr(QStringLiteral("settings.theme.desc")),
                       themePicker, SettingRow::Trailing),
        new SettingRow(Locale::tr(QStringLiteral("settings.language.label")),
                       Locale::tr(QStringLiteral("settings.language.desc")),
                       language, SettingRow::Trailing),
        new SettingRow(Locale::tr(QStringLiteral("settings.typeface.label")),
                       Locale::tr(QStringLiteral("settings.typeface.desc")),
                       typeface, SettingRow::Trailing),
        new SettingRow(Locale::tr(QStringLiteral("settings.fontsize.label")),
                       Locale::tr(QStringLiteral("settings.fontsize.desc")),
                       fontSize, SettingRow::Trailing),
        new SettingRow(Locale::tr(QStringLiteral("settings.accent.label")),
                       Locale::tr(QStringLiteral("settings.accent.desc")),
                       accent, SettingRow::Trailing),
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
    return makeSection(Locale::tr(QStringLiteral("settings.section.appearance")), rows, this);
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
        Locale::tr(QStringLiteral("settings.update.desc")).arg(Updater::repository()),
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
        new SettingRow(Locale::tr(QStringLiteral("settings.update.onLaunch.label")),
                       Locale::tr(QStringLiteral("settings.update.onLaunch.desc")),
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
        if (!QProcess::startDetached(QStringLiteral("SystemPropertiesProtection.exe"), {}))
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

    QHash<QString, int> positions;
    for (const Category &c : Catalog::instance().categories())
        for (const Section &s : c.sections)
            for (const Tweak &t : s.tweaks)
                positions.insert(t.id, m_state->selected(t.id));

    QString error;
    const QString name = QFileInfo(path).completeBaseName();
    if (Preset::save(path, name, positions, &error))
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
