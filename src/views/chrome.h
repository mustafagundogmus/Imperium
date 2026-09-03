// chrome.h — what a window shell is, seen from MainWindow.
//
// The pages — overview, tweak list, settings, journal, cleaner and the rest — are built
// once and live in one QStackedWidget. Everything around that stack is the chrome: the
// title bar, the navigation, the header over the page, the bar that applies. Two of
// them exist, the classic sidebar shell and the Fluent rail-and-pane one, and the window
// talks to whichever is wearing the stack through this interface so that swapping them
// is a rebuild of the chrome and nothing else.
//
// A Chrome owns the widgets it creates on the card and deletes them with itself; the
// card's root layout is the window's to replace.

#pragma once

#include <QObject>
#include <QString>

class QWidget;
struct Sample;

class Chrome : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Lays this shell out on \a card around \a stack, which the shell reparents as it
    /// needs. \a card has no layout when this is called.
    virtual void build(QWidget *card, QWidget *stack) = 0;

    /// The strip the apply overlay leaves uncovered, so the window stays movable.
    virtual int titleBarHeight() const = 0;

    virtual void setMaximized(bool maximized) = 0;
    virtual void setSystemSummary(const QString &summary) { Q_UNUSED(summary); }

    // navigation
    virtual void setSelected(const QString &id) = 0;
    virtual void setCategoryCount(const QString &id, const QString &text) = 0;

    // the header over the page
    virtual void setTitle(const QString &title) = 0;
    virtual void setSubtitle(const QString &subtitle) = 0;
    virtual void setPendingLabel(const QString &label) = 0;
    virtual void setControlsVisible(bool visible) = 0;
    /// The three filter positions' counts for the page on screen. The classic header
    /// has no place for them.
    virtual void setFilterCounts(int all, int enabled, int changed)
    { Q_UNUSED(all); Q_UNUSED(enabled); Q_UNUSED(changed); }

    // the bar that applies
    virtual void setPending(int count) = 0;
    virtual void setSummary(const QString &summary) { Q_UNUSED(summary); }
    virtual void setNotice(const QString &text) = 0;

    // search
    virtual QString searchText() const = 0;
    virtual void setSearchText(const QString &text) = 0;
    virtual void clearSearch() = 0;
    virtual void focusSearch() = 0;

    // the live machine, for a shell that shows it
    virtual void setSample(const Sample &sample) { Q_UNUSED(sample); }
    virtual void setRestorePoint(const QString &text) { Q_UNUSED(text); }

Q_SIGNALS:
    void categoryActivated(const QString &id);
    void queryChanged(const QString &query);
    /// An AppState::Filter, as an int.
    void filterChanged(int filter);
    void sortToggled(bool alphabetical);
    void applyRequested();
    void revertRequested();
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();
    void restorePointRequested();
};
