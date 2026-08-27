// tilauncherpage.h — the TrustedInstaller Launcher screen.
//
// One target field and a Browse button, a row of quick launches for the tools people
// actually want at this level — a shell, PowerShell, the registry editor, the file
// manager — and a short, honest note about what "TrustedInstaller" means. It is built from
// the same section headers and rows as the rest of the app, so it reads as part of it.
//
// The launch itself is native and lives in src/trustedinstaller.*; this file is only the
// screen. When the app is not elevated the whole page is dimmed and says why, because a
// standard token cannot reach TrustedInstaller no matter how the launch is phrased.

#pragma once

#include <QVector>
#include <QWidget>

class PillButton;
class SectionHeader;
class SettingRow;
class QLabel;
class QLineEdit;

class TiLauncherPage : public QWidget
{
    Q_OBJECT

public:
    explicit TiLauncherPage(QWidget *parent = nullptr);

Q_SIGNALS:
    void notice(const QString &text);

private:
    /// Runs one target through TrustedInstaller::launch and reports the outcome on \a row
    /// (when given) and the status bar.
    void launch(const QString &program, const QString &arguments, SettingRow *row);
    void launchFromField();
    void browse();
    void applyInputStyle();
    void refreshAvailability();
    void retranslate();

    struct Quick
    {
        QString id;
        QString program;   ///< resolved absolute path
        SettingRow *row = nullptr;
        PillButton *button = nullptr;
    };

    SectionHeader *m_introHeader = nullptr;
    QLabel *m_intro = nullptr;
    SectionHeader *m_targetHeader = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLineEdit *m_path = nullptr;
    PillButton *m_browse = nullptr;
    QLabel *m_argsLabel = nullptr;
    QLineEdit *m_args = nullptr;
    PillButton *m_launch = nullptr;
    SectionHeader *m_quickHeader = nullptr;
    QVector<Quick> m_quick;
    bool m_available = true;
};
