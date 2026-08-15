// settingspage.h — the Ayarlar screen.
//
// Built from the same parts as the tweak list (section headers, 49px rows, the 26×15
// switch, the segmented control, the pill buttons) so it belongs to the app rather than
// looking like a settings dialog bolted on the side.

#pragma once

#include <QWidget>

class AppState;
class PillButton;
class SettingRow;
class Updater;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    SettingsPage(AppState *state, Updater *updater, QWidget *parent = nullptr);

    /// Number of rows, for the header subtitle.
    int rowCount() const { return m_rowCount; }

Q_SIGNALS:
    /// A preset was loaded and its states pushed into AppState.
    void presetApplied(const QString &name, int applied, int unknown);
    void notice(const QString &text);

private Q_SLOTS:
    void onExportPreset();
    void onImportPreset();
    void onCheckUpdates();

private:
    QWidget *buildAppearance();
    QWidget *buildPresets();
    QWidget *buildApplication();
    QWidget *buildSafety();

    AppState *m_state = nullptr;
    Updater *m_updater = nullptr;

    SettingRow *m_updateRow = nullptr;
    PillButton *m_updateButton = nullptr;
    SettingRow *m_presetRow = nullptr;
    int m_rowCount = 0;
};
