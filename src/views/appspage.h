// appspage.h — the Uygulama kur screen: WinUtil's Install tab.
//
// Two hundred and thirty-three programs in ten categories, each a tile that flows in a
// wrap panel under a collapsible heading; a search box and a row of category chips
// that filter them together; and the tab's own controls — Install/Upgrade, Uninstall,
// Upgrade all, the WinGet/Chocolatey preference, Clear selection, Show installed,
// Collapse/Expand all, the "Selected apps: N" popup, and the import/export of a
// selection in WinUtil's own preset format, so a file made there loads here.
//
// The machine is only touched through Apps::Runner, one run at a time, and every run
// that changes it is confirmed first with the exact commands it will issue — the same
// promise the actions page and the debloat page keep. Progress comes back package by
// package and is drawn on a strip above the list and on the tiles themselves; the raw
// output of winget and choco streams into a log panel that opens on request.

#pragma once

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "../apps.h"

class AppEntry;
class FlowLayout;
class PillButton;
class QLabel;
class QPlainTextEdit;
class QTimer;
class QVBoxLayout;
class SearchField;
class SegmentedControl;

class AppsPage : public QWidget
{
    Q_OBJECT

public:
    explicit AppsPage(QWidget *parent = nullptr);

    int rowCount() const;
    int categoryCount() const;
    int selectedCount() const { return int(m_selected.size()); }

    /// A run — install, uninstall, detection, repair — is in flight.
    bool busy() const;

    /// The line under the page title: what is in flight when something is, else the
    /// catalogue's counts and the selection.
    QString subtitle() const;

Q_SIGNALS:
    void notice(const QString &text);
    /// The selection or the run state changed; the header's subtitle follows it.
    void stateChanged();

private:
    class Chip;
    class CategoryHeader;
    class ProgressStrip;
    class SelectionPopup;
    class EntryPopup;

    struct Block
    {
        QString category;
        CategoryHeader *header = nullptr;
        QWidget *panel = nullptr;
        FlowLayout *flow = nullptr;
        QVector<AppEntry *> entries;
        bool collapsed = false;
    };

    void buildToolbar();
    void buildFilters();
    void buildList();
    void retranslate();

    // selection
    void setSelected(const QString &key, bool on);
    void clearSelection();
    void selectionChanged();
    void showSelectionPopup();

    // filtering — Find-AppsByNameOrDescription and the chips
    void applyFilter();
    void onChipClicked(const QString &category, bool additive);
    void syncChips();
    void setAllCollapsed(bool collapsed);
    void toggleBlock(Block &block);

    // runs
    QVector<Apps::Entry> selectedEntries() const;
    void installSelected();
    void uninstallSelected();
    void installOne(const QString &key);
    void uninstallOne(const QString &key);
    void runBatch(const QVector<Apps::Entry> &packages, Apps::Operation op);
    void upgradeAll();
    void showInstalled();
    void repairManager();
    void importSelection();
    void exportSelection();
    void openLink(const QString &key);

    // runner feedback
    void onStarted(Apps::Runner::Job job, int total);
    void onProgress(int done, int total, const QString &id, bool finished, int exitCode);
    void onLine(const QString &text);
    void onFinished(Apps::Runner::Job job, bool ok, int failures);
    void onDetected(const QStringList &keys);
    void onProbed(bool winget, bool choco);
    void setRunControlsEnabled(bool enabled);
    void refreshStatusLine();

    Apps::Manager preference() const { return m_preference; }
    void setPreference(Apps::Manager m);
    QString describeProgress(const QString &id, bool finished, int done, int total) const;
    QStringList keysForId(const QString &id) const;

    Apps::Runner *m_runner = nullptr;
    Apps::Manager m_preference = Apps::Manager::Winget;

    QVBoxLayout *m_layout = nullptr;

    // toolbar
    PillButton *m_installButton = nullptr;
    PillButton *m_uninstallButton = nullptr;
    PillButton *m_upgradeButton = nullptr;
    SegmentedControl *m_managerControl = nullptr;
    QLabel *m_managerLabel = nullptr;
    PillButton *m_selectedButton = nullptr;
    PillButton *m_clearButton = nullptr;
    PillButton *m_installedButton = nullptr;
    PillButton *m_collapseButton = nullptr;
    PillButton *m_expandButton = nullptr;
    PillButton *m_importButton = nullptr;
    PillButton *m_exportButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    PillButton *m_repairButton = nullptr;
    PillButton *m_logButton = nullptr;
    QPlainTextEdit *m_log = nullptr;

    // filters
    SearchField *m_search = nullptr;
    QTimer *m_searchTimer = nullptr;
    QLabel *m_filterHint = nullptr;
    QVector<Chip *> m_chips;          ///< the All chip first, then one per category
    QSet<QString> m_activeCategories; ///< empty = all
    QSet<QString> m_autoExpanded;     ///< categories the filter opened on the user's behalf
    QLabel *m_fossLegend = nullptr;

    // list
    ProgressStrip *m_progress = nullptr;
    QWidget *m_body = nullptr;
    QVector<Block> m_blocks;
    QHash<QString, AppEntry *> m_entries;   ///< key → tile
    QSet<QString> m_selected;               ///< keys, in no order
    QStringList m_pendingKeys;              ///< tiles the run in flight covers
    QHash<QString, QStringList> m_idToKeys; ///< package id → the keys it stands for

    // manager availability, from the probe
    bool m_probed = false;
    bool m_wingetPresent = false;
    bool m_chocoPresent = false;

    QString m_progressText;   ///< the strip's current sentence, for subtitle()
    SelectionPopup *m_selectionPopup = nullptr;
    EntryPopup *m_entryPopup = nullptr;
};
