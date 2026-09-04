// tilauncherpage.h — the TrustedInstaller Launcher screen.
//
// One target field and a Browse button; the targets launched from it, kept as rows of
// their own ("Son hedefler", eight at most, in the registry with the other settings); a
// row of quick launches for the tools people actually want at this level — a shell,
// PowerShell, the registry editor, the file manager, and the consoles: Task Manager,
// Services, Device Manager, Task Scheduler, Event Viewer, Disk and Computer Management,
// Group Policy where the edition has it; and a short, honest note about what
// "TrustedInstaller" means. A file dropped anywhere on the page lands in the target
// field, and the field takes a bare command name as well as a path — "notepad" is found
// the way a shell would find it (see TrustedInstaller::resolve). It is built from the same
// section headers and rows as the rest of the app, so it reads as part of it.
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
class QVBoxLayout;

class TiLauncherPage : public QWidget
{
    Q_OBJECT

public:
    explicit TiLauncherPage(QWidget *parent = nullptr);

Q_SIGNALS:
    void notice(const QString &text);

protected:
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dropEvent(QDropEvent *) override;

private:
    /// One remembered launch from the target field.
    struct Recent
    {
        QString program;     ///< resolved, absolute
        QString arguments;
    };

    /// Runs one target through TrustedInstaller::launch and reports the outcome on \a row
    /// (when given) and the status bar. \a resolved receives the file that was started.
    bool launch(const QString &program, const QString &arguments, SettingRow *row,
                QString *resolved = nullptr);
    void launchFromField();
    void browse();
    void applyInputStyle();
    void refreshAvailability();
    void retranslate();
    void remember(const QString &program, const QString &arguments);
    void rebuildRecent();
    static QVector<Recent> loadRecent();
    static void saveRecent(const QVector<Recent> &recent);

    struct Quick
    {
        QString id;
        QString program;     ///< resolved absolute path
        QString arguments;   ///< the console an mmc row opens
        SettingRow *row = nullptr;
        PillButton *button = nullptr;
    };

    SectionHeader *m_introHeader = nullptr;
    QLabel *m_intro = nullptr;
    SectionHeader *m_targetHeader = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLineEdit *m_path = nullptr;
    PillButton *m_browse = nullptr;
    QLabel *m_hint = nullptr;
    QLabel *m_argsLabel = nullptr;
    QLineEdit *m_args = nullptr;
    PillButton *m_launch = nullptr;
    QWidget *m_recentBlock = nullptr;
    SectionHeader *m_recentHeader = nullptr;
    QWidget *m_recentList = nullptr;
    QVBoxLayout *m_recentLayout = nullptr;
    PillButton *m_recentClear = nullptr;
    QVector<SettingRow *> m_recentRows;
    QVector<Recent> m_recent;
    SectionHeader *m_quickHeader = nullptr;
    QVector<Quick> m_quick;
    bool m_available = true;
};
