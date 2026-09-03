// classicchrome.h — the sidebar shell, as one Chrome.
//
//   TitleBar 36px · Sidebar 212px · ContentHeader over the stack · StatusBar 36px
//
// Nothing here is new: the four widgets are the ones MainWindow used to place itself.
// This class is the plumbing that used to be inline there, so that the Fluent shell
// could be a sibling rather than a fork.

#pragma once

#include "chrome.h"

#include <QPointer>

class AppState;
class ContentHeader;
class Sidebar;
class StatusBar;
class TitleBar;

class ClassicChrome : public Chrome
{
    Q_OBJECT

public:
    explicit ClassicChrome(AppState *state, QObject *parent = nullptr);
    ~ClassicChrome() override;

    void build(QWidget *card, QWidget *stack) override;
    int titleBarHeight() const override;

    void setMaximized(bool maximized) override;
    void setSystemSummary(const QString &summary) override;
    void setSelected(const QString &id) override;
    void setCategoryCount(const QString &id, const QString &text) override;
    void setTitle(const QString &title) override;
    void setSubtitle(const QString &subtitle) override;
    void setPendingLabel(const QString &label) override;
    void setControlsVisible(bool visible) override;
    void setPending(int count) override;
    void setSummary(const QString &summary) override;
    void setNotice(const QString &text) override;
    QString searchText() const override;
    void setSearchText(const QString &text) override;
    void clearSearch() override;
    void focusSearch() override;

private:
    // QPointer for the same reason FluentChrome's are: the card deletes them first when
    // the window closes.
    AppState *m_state = nullptr;
    QPointer<TitleBar> m_titleBar;
    QPointer<Sidebar> m_sidebar;
    QPointer<ContentHeader> m_header;
    QPointer<StatusBar> m_statusBar;
};
