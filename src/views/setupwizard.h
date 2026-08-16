// setupwizard.h — first-launch setup.
//
// Shown once, before MainWindow: whoever opens Arbitrium for the first time on a machine
// picks a language and a look, in three short steps. Locale::markSetupComplete() is
// called when it finishes, so every later launch skips straight past it.
//
// Built on FramelessWindow rather than a QDialog so it carries the app's own chrome (the
// card, the shadow, TitleBar's drag-to-move) instead of looking like a system dialog
// bolted onto a themed app. Closing it from the titlebar's × counts as finishing with
// whatever was chosen up to that point — it does not block first launch, since the
// choices it makes are all cosmetic and every one of them is still in Ayarlar afterwards.

#pragma once

#include "../framelesswindow.h"

class PillButton;
class QLabel;
class QStackedWidget;
class TitleBar;

class SetupWizard : public FramelessWindow
{
    Q_OBJECT

public:
    explicit SetupWizard(QWidget *parent = nullptr);

Q_SIGNALS:
    /// Setup is done — proceed to MainWindow.
    void finished();

private:
    QWidget *buildLanguagePage();
    QWidget *buildAppearancePage();
    QWidget *buildFinishPage();
    void showPage(int index);
    void retranslate();

    TitleBar *m_titleBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    PillButton *m_back = nullptr;
    PillButton *m_next = nullptr;

    QLabel *m_title1 = nullptr;
    QLabel *m_subtitle1 = nullptr;
    QLabel *m_title2 = nullptr;
    QLabel *m_subtitle2 = nullptr;
    QLabel *m_title3 = nullptr;
    QLabel *m_subtitle3 = nullptr;
    QLabel *m_captionTheme = nullptr;
    QLabel *m_captionAccent = nullptr;
    QLabel *m_captionTypeface = nullptr;

    int m_page = 0;
};
