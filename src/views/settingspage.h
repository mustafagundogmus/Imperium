// settingspage.h — the Ayarlar screen.
//
// Built from the same parts as the tweak list (section headers, 49px rows, the 30×16
// switch, the segmented control, the pill buttons) so it belongs to the app rather than
// looking like a settings dialog bolted on the side.
//
// Five sections, grouped by what somebody is looking for rather than by what was added
// when:
//
//   Görünüm   dil, tema, vurgu rengi, yazı tipi, yazı boyutu — the five settings that
//             offer a choice from a set. Each is a caption over its gallery
//             (SettingRow::Below), and every gallery wraps to the column it is given, so
//             none of them elides an option away at any text size or window width.
//   Arayüz    kompakt satırlar, akıcı kaydırma, kenar parıltısı — the three switches.
//             They have no options to show and do not belong among the galleries.
//   Ön ayarlar / Uygulama / Güvenlik — unchanged, and already lists of plain rows.
//
// One shape per section is the whole point: what made the old page look jumbled was a
// 160px theme grid, a two-row chip cloud, a 20px swatch strip and three 40px switch rows
// stacked into a single list with nothing to say which of them belonged together.
//
// The whole page is thrown away and rebuilt on a language change rather than having every
// row learn to retranslate itself in place: nineteen-odd rows, mostly title+description
// pairs, is a lot of individual setters to add and keep in sync for a screen the user is
// not staring at when it happens the vast majority of the time — the one exception being
// the language row itself, which is why the rebuild has to be immediate rather than
// deferred to the next visit.

#pragma once

#include <QWidget>

class AppState;
class PillButton;
class QVBoxLayout;
class SettingRow;
class Updater;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    SettingsPage(AppState *state, Updater *updater, QWidget *parent = nullptr);

    /// Number of rows, for the header subtitle.
    int rowCount() const { return m_rowCount; }

    /// Last system restore point, as SysInfo's background probe reports it. Empty means
    /// the machine has none. Shown on the Güvenlik row that opens System Protection.
    void setRestorePoint(const QString &value);

Q_SIGNALS:
    /// A preset was loaded and its states pushed into AppState.
    void presetApplied(const QString &name, int applied, int unknown);
    void notice(const QString &text);

private Q_SLOTS:
    void onExportPreset();
    void onImportPreset();
    void onCheckUpdates();
    void onExportReg();

private:
    void rebuild();
    QWidget *buildAppearance();
    QWidget *buildInterface();
    QWidget *buildPresets();
    QWidget *buildApplication();
    QWidget *buildSafety();

    AppState *m_state = nullptr;
    Updater *m_updater = nullptr;
    QVBoxLayout *m_layout = nullptr;

    SettingRow *m_updateRow = nullptr;
    PillButton *m_updateButton = nullptr;
    SettingRow *m_presetRow = nullptr;
    SettingRow *m_restoreRow = nullptr;
    QString m_restoreValue;         ///< last value passed to setRestorePoint(), replayed after a rebuild
    bool m_restoreKnown = false;    ///< false until the first probe result arrives
    int m_rowCount = 0;
};
