// debloatpage.h — the preinstalled-apps screen.
//
// The list is the machine's own inventory of image-provisioned packages, not a curated
// selection: each one is named as Windows names it, with the logo out of its own manifest.
// Packages Windows marks non-removable appear too, locked rather than hidden, so the page
// can be read as a complete answer to "what did this Windows come with".
//
// A removal (single row or the bulk bar) always shows its script and asks first, exactly
// like ActionPage's confirmAndRun; the same ActionEngine class runs it. Language changes
// rebuild from the last scan rather than re-running PowerShell — a rescan happens on
// demand, or after a removal actually changes the machine.

#pragma once

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "../debloat.h"

class ActionEngine;
class DebloatRow;
class DebloatScanner;
class PillButton;
class QLabel;
class QVBoxLayout;
class SectionHeader;

class DebloatPage : public QWidget
{
    Q_OBJECT

public:
    explicit DebloatPage(QWidget *parent = nullptr);

    /// Packages found on this run's scan — 0 while the first scan is still in flight.
    int rowCount() const { return m_rowCount; }

    /// …of which these many are ones the user may actually act on.
    int removableCount() const { return m_removableCount; }

    bool scanning() const { return m_scanning; }

Q_SIGNALS:
    void notice(const QString &text);
    void scanFinished();

private:
    void rescan();
    void rebuild(const QVector<InstalledApp> &apps);
    void updateBulkBar();
    void toggleSelectAll();
    void removeOne(const InstalledApp &app);
    void removeSelected();
    void runRemoval(const QStringList &ids, const QStringList &packageNames,
                    const QString &confirmText);
    void retranslate();

    DebloatScanner *m_scanner = nullptr;
    ActionEngine *m_engine = nullptr;

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_body = nullptr;

    QLabel *m_summaryLabel = nullptr;
    PillButton *m_rescanButton = nullptr;
    PillButton *m_selectAllButton = nullptr;
    PillButton *m_bulkRemoveButton = nullptr;

    QHash<QString, DebloatRow *> m_rows;       ///< package name -> its row
    QVector<SectionHeader *> m_sectionHeaders;
    QStringList m_pendingIds;                  ///< rows the engine's current run covers
    QVector<InstalledApp> m_lastApps;          ///< kept for retranslate() — no rescan needed

    int m_rowCount = 0;
    int m_removableCount = 0;
    bool m_scanning = true;
};
