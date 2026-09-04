// fluentchrome.h — the Windows 11 shell from design_handoff_fluent_ui, as one Chrome.
//
//   IconRail 56px | CategoryPane 232px | FluentTitleBar 48px over FluentContent
//                                        (FluentHeader, the stack, ApplyBar)
//
// The rail and the pane run the full height; the title bar spans the content column only,
// its logo centred there, so the search box and the first rail button take the top-left
// corner. The rail is an overlay the chrome places (placeRail), not a layout item: it
// widens over the pane while the pointer is on it — see iconrail.h.
//
// The rail's six entries are the handoff's — Genel Bakış, Tweakler, Araçlar, Paketler,
// Geçmiş, Güvenlik — and the cog at its foot. Each opens a set of pages in the pane:
// Tweakler every catalogue category, Araçlar the four tools, Paketler the app list,
// Geçmiş the journal, Güvenlik the two categories a security-minded user reads together,
// the cog Ayarlar and Hakkında. The pane remembers the last page opened under each rail
// entry, so pressing a rail button takes you back where you were in it.

#pragma once

#include "../views/chrome.h"

#include <QHash>
#include <QPointer>
#include <QVector>

class AppState;
class ApplyBar;
class CategoryPane;
namespace Icons { struct Glyph; }
class FluentContent;
class FluentHeader;
class FluentTitleBar;
class IconRail;

class FluentChrome : public Chrome
{
    Q_OBJECT

public:
    explicit FluentChrome(AppState *state, QObject *parent = nullptr);
    ~FluentChrome() override;

    void build(QWidget *card, QWidget *stack) override;
    int titleBarHeight() const override;

    void setMaximized(bool maximized) override;
    void setSelected(const QString &id) override;
    void setCategoryCount(const QString &id, const QString &text) override;
    void setTitle(const QString &title) override;
    void setSubtitle(const QString &subtitle) override;
    void setPendingLabel(const QString &label) override;
    void setControlsVisible(bool visible) override;
    void setFilterCounts(int all, int enabled, int changed) override;
    void setPending(int count) override;
    void setNotice(const QString &text) override;
    QString searchText() const override;
    void setSearchText(const QString &text) override;
    void clearSearch() override;
    void focusSearch() override;
    void setSample(const Sample &sample) override;
    void setRestorePoint(const QString &text) override;

protected:
    /// The card's resizes, to keep the rail overlay the card's height.
    bool eventFilter(QObject *watched, QEvent *e) override;

private:
    struct Rail
    {
        QString labelKey;
        const Icons::Glyph *glyph = nullptr;
        QVector<QString> ids;
    };

    void buildRails();
    void placeRail();
    void retranslate();
    int railOf(const QString &id, int preferred) const;
    QString railLabel(int index) const;
    QString pageLabel(const QString &id) const;
    void showRail(int index);
    void onRailActivated(int index);
    void composeSubtitle();

    // QPointer, because the card deletes these as its children when the window goes
    // down, and the chrome — a QObject child of the window — is destroyed after that.
    // A chrome replaced at run time is the case where they are still alive here.
    AppState *m_state = nullptr;
    QPointer<FluentTitleBar> m_titleBar;
    QPointer<IconRail> m_rail;
    QPointer<CategoryPane> m_pane;
    QPointer<FluentHeader> m_header;
    QPointer<ApplyBar> m_bar;
    QPointer<FluentContent> m_content;

    QVector<Rail> m_rails;
    Rail m_settingsRail;
    int m_currentRail = 0;             ///< -1 for the cog
    QString m_selected;
    QString m_subtitle;
    QString m_pendingLabel;
    QHash<int, QString> m_lastInRail;
    QHash<QString, QString> m_counts;  ///< live counts the catalogue cannot answer
};
