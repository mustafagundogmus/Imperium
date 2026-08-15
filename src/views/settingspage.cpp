#include "settingspage.h"

#include "../appstate.h"
#include "../catalog.h"
#include "../preset.h"
#include "../settings.h"
#include "../theme.h"
#include "../tweakengine.h"
#include "../updater.h"
#include "../widgets/accentpicker.h"
#include "../widgets/buttons.h"
#include "../widgets/sectionheader.h"
#include "../widgets/segmentedcontrol.h"
#include "../widgets/settingrow.h"
#include "../widgets/toggleswitch.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int PadLeft = 18;
constexpr int PadTop = 2;
constexpr int PadRight = 12;
constexpr int PadBottom = 16;

/// A section is a header plus 1px-separated rows, exactly like a tweak section.
QWidget *makeSection(const QString &title, const QVector<QWidget *> &rows, QWidget *parent)
{
    auto *block = new QWidget(parent);
    auto *layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new SectionHeader(title, block);
    header->setCount(QStringLiteral("%1 öğe").arg(rows.size()));
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
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(PadLeft, PadTop, PadRight, PadBottom);
    outer->setSpacing(Theme::Metric::SectionGap);

    outer->addWidget(buildAppearance());
    outer->addWidget(buildPresets());
    outer->addWidget(buildApplication());
    outer->addWidget(buildSafety());
    outer->addStretch(1);

    connect(m_updater, &Updater::finished, this,
            [this](bool available, const QString &version, const QString &url, const QString &error) {
                m_updateButton->setEnabledLook(true);
                if (!error.isEmpty()) {
                    m_updateRow->setDesc(QStringLiteral("Denetlenemedi · %1").arg(error));
                    Q_EMIT notice(QStringLiteral("güncelleme denetimi başarısız"));
                    return;
                }
                if (available) {
                    m_updateRow->setDesc(QStringLiteral("Yeni sürüm var: %1 — indirmek için tıklayın.").arg(version));
                    Q_EMIT notice(QStringLiteral("yeni sürüm: %1").arg(version));
                    QDesktopServices::openUrl(QUrl(url));
                } else {
                    m_updateRow->setDesc(QStringLiteral("Güncel: %1 sürümünü kullanıyorsunuz.")
                                             .arg(QCoreApplication::applicationVersion()));
                    Q_EMIT notice(QStringLiteral("en güncel sürümdesiniz"));
                }
            });
}

QWidget *SettingsPage::buildAppearance()
{
    auto *themePicker = new SegmentedControl({QStringLiteral("Koyu"), QStringLiteral("Açık")});
    themePicker->setCurrentIndex(Theme::appearance() == Theme::Appearance::Light ? 1 : 0);
    connect(themePicker, &SegmentedControl::currentIndexChanged, this, [](int index) {
        Theme::setAppearance(index == 1 ? Theme::Appearance::Light : Theme::Appearance::Dark);
    });

    auto *accent = new AccentPicker;
    connect(accent, &AccentPicker::picked, this, [](const QColor &c) { Theme::setAccent(c); });

    auto *compact = new ToggleSwitch;
    compact->setChecked(Theme::compact(), false);
    connect(compact, &ToggleSwitch::toggled, this, [](bool on) { Theme::setCompact(on); });

    auto *smooth = new ToggleSwitch;
    smooth->setChecked(Settings::instance().smoothScroll(), false);
    connect(smooth, &ToggleSwitch::toggled, this, [](bool on) { Settings::instance().setSmoothScroll(on); });

    const QVector<QWidget *> rows{
        new SettingRow(QStringLiteral("Tema"),
                       QStringLiteral("Koyu palet tasarımın kendisidir; açık palet aynı ton ilişkileriyle türetilmiştir."),
                       themePicker, SettingRow::Trailing),
        new SettingRow(QStringLiteral("Vurgu rengi"),
                       QStringLiteral("Anahtarlar, seçili kategori ve Uygula düğmesi bu rengi kullanır."),
                       accent, SettingRow::Trailing),
        new SettingRow(QStringLiteral("Kompakt satırlar"),
                       QStringLiteral("Tweak satırlarının iç boşluğunu 7px yerine 4px yapar."),
                       compact, SettingRow::Leading),
        new SettingRow(QStringLiteral("Akıcı kaydırma"),
                       QStringLiteral("Tekerlek hareketini basamaklar yerine yumuşak geçişle uygular."),
                       smooth, SettingRow::Leading),
    };
    m_rowCount += rows.size();
    return makeSection(QStringLiteral("Görünüm"), rows, this);
}

QWidget *SettingsPage::buildPresets()
{
    auto *save = new PillButton(PillButton::Ghost, QStringLiteral("Dışa aktar"));
    auto *load = new PillButton(PillButton::Ghost, QStringLiteral("İçe aktar"));
    connect(save, &PillButton::clicked, this, &SettingsPage::onExportPreset);
    connect(load, &PillButton::clicked, this, &SettingsPage::onImportPreset);

    auto *openFolder = new PillButton(PillButton::Ghost, QStringLiteral("Klasörü aç"));
    connect(openFolder, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Preset::directory()));
    });

    m_presetRow = new SettingRow(
        QStringLiteral("Ön ayar dosyası"),
        QStringLiteral("Anahtar seçimlerinizi XML olarak kaydeder ve geri yükler."),
        makePair(save, load), SettingRow::Trailing);

    const QVector<QWidget *> rows{
        m_presetRow,
        new SettingRow(QStringLiteral("Ön ayar klasörü"),
                       QDir::toNativeSeparators(Preset::directory()),
                       openFolder, SettingRow::Trailing),
    };
    m_rowCount += rows.size();
    return makeSection(QStringLiteral("Ön ayarlar"), rows, this);
}

QWidget *SettingsPage::buildApplication()
{
    m_updateButton = new PillButton(PillButton::Ghost, QStringLiteral("Denetle"));
    connect(m_updateButton, &PillButton::clicked, this, &SettingsPage::onCheckUpdates);

    m_updateRow = new SettingRow(
        QStringLiteral("Güncellemeleri denetle"),
        QStringLiteral("%1 deposundaki en son sürümle karşılaştırır.").arg(Updater::repository()),
        m_updateButton, SettingRow::Trailing);

    auto *onLaunch = new ToggleSwitch;
    onLaunch->setChecked(Settings::instance().checkUpdatesOnLaunch(), false);
    connect(onLaunch, &ToggleSwitch::toggled, this,
            [](bool on) { Settings::instance().setCheckUpdatesOnLaunch(on); });

    auto *openRepo = new PillButton(PillButton::Ghost, QStringLiteral("Depoyu aç"));
    connect(openRepo, &PillButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(Updater::releasesUrl())); });

    const QVector<QWidget *> rows{
        m_updateRow,
        new SettingRow(QStringLiteral("Açılışta denetle"),
                       QStringLiteral("Uygulama açılırken sessizce yeni sürüm arar."),
                       onLaunch, SettingRow::Leading),
        new SettingRow(QStringLiteral("Sürüm"),
                       QStringLiteral("Arbitrium %1 · Qt %2 · %3 tweak")
                           .arg(QCoreApplication::applicationVersion(), QT_VERSION_STR)
                           .arg(Catalog::instance().totalTweaks()),
                       openRepo, SettingRow::Trailing),
    };
    m_rowCount += rows.size();
    return makeSection(QStringLiteral("Uygulama"), rows, this);
}

QWidget *SettingsPage::buildSafety()
{
    auto *confirm = new ToggleSwitch;
    confirm->setChecked(Settings::instance().confirmBeforeApply(), false);
    connect(confirm, &ToggleSwitch::toggled, this,
            [](bool on) { Settings::instance().setConfirmBeforeApply(on); });

    auto *openJournal = new PillButton(PillButton::Ghost, QStringLiteral("Günlüğü aç"));
    connect(openJournal, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    });

    auto *restorePoint = new PillButton(PillButton::Ghost, QStringLiteral("Sistem koruması"));
    connect(restorePoint, &PillButton::clicked, this, [] {
        if (!QProcess::startDetached(QStringLiteral("SystemPropertiesProtection.exe"), {}))
            QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:about")));
    });

    QVector<QWidget *> rows{
        new SettingRow(QStringLiteral("Uygulamadan önce onay iste"),
                       QStringLiteral("Uygula'ya basınca hangi anahtarların yazılacağını önce sorar."),
                       confirm, SettingRow::Leading),
        new SettingRow(QStringLiteral("Değişiklik günlüğü"),
                       QStringLiteral("Her yazma öncesi eski değer kaydedilir; klasörü açar."),
                       openJournal, SettingRow::Trailing),
        new SettingRow(QStringLiteral("Geri yükleme noktası"),
                       QStringLiteral("Windows'un kendi Sistem Koruması penceresini açar."),
                       restorePoint, SettingRow::Trailing),
    };

    if (!TweakEngine::isElevated()) {
        auto *elevate = new PillButton(PillButton::Ghost, QStringLiteral("Yeniden başlat"));
        connect(elevate, &PillButton::clicked, this, [this] {
            m_state->stashPending();
            if (TweakEngine::relaunchElevated())
                window()->close();
        });
        rows.append(new SettingRow(QStringLiteral("Yönetici olarak çalıştır"),
                                   QStringLiteral("HKLM altındaki tweak'ler yönetici yetkisi ister."),
                                   elevate, SettingRow::Trailing));
    }

    m_rowCount += rows.size();
    return makeSection(QStringLiteral("Güvenlik"), rows, this);
}

void SettingsPage::onExportPreset()
{
    const QString suggested = QDir(Preset::directory())
                                  .filePath(Preset::fileNameFor(QStringLiteral("onayar")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Ön ayarı dışa aktar"), suggested,
        QStringLiteral("Arbitrium ön ayarı (*.xml)"));
    if (path.isEmpty())
        return;

    QHash<QString, bool> states;
    for (const Category &c : Catalog::instance().categories())
        for (const Section &s : c.sections)
            for (const Tweak &t : s.tweaks)
                states.insert(t.id, m_state->isOn(t.id));

    QString error;
    const QString name = QFileInfo(path).completeBaseName();
    if (Preset::save(path, name, states, &error))
        Q_EMIT notice(QStringLiteral("ön ayar kaydedildi · %1 tweak").arg(states.size()));
    else
        Q_EMIT notice(QStringLiteral("kaydedilemedi · %1").arg(error));
}

void SettingsPage::onImportPreset()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Ön ayar içe aktar"), Preset::directory(),
        QStringLiteral("Arbitrium ön ayarı (*.xml)"));
    if (path.isEmpty())
        return;

    const Preset::LoadResult result = Preset::load(path);
    if (!result.ok) {
        Q_EMIT notice(QStringLiteral("okunamadı · %1").arg(result.error));
        return;
    }

    // A preset only moves switches; nothing is written until the user presses Uygula.
    int changed = 0;
    for (auto it = result.states.cbegin(); it != result.states.cend(); ++it) {
        if (m_state->isOn(it.key()) == it.value())
            continue;
        m_state->setOn(it.key(), it.value());
        ++changed;
    }

    Q_EMIT presetApplied(result.meta.name.isEmpty() ? QFileInfo(path).completeBaseName()
                                                    : result.meta.name,
                         changed, result.unknownIds);
}

void SettingsPage::onCheckUpdates()
{
    m_updateButton->setEnabledLook(false);
    m_updateRow->setDesc(QStringLiteral("Denetleniyor…"));
    m_updater->check();
}
