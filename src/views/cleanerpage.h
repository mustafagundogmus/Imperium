// cleanerpage.h — the Disk cleaner screen.
//
// Sixteen targets in four groups, each measured in the background the moment the page
// is built and again after every clean, so the number on the right of a row and the
// figure in the sidebar are always what a clean would actually free — not what a
// hard-coded list says a temp folder usually holds.
//
// The page asks before it deletes, the way every other destructive surface in the app
// does, and the confirmation's detail panel lists exactly which targets and how much. The
// heavier targets sit in their own group and start unchecked: a previous Windows
// installation is a way back that ends the day it is deleted, and old restore points are
// the same, so those are a decision rather than a default.

#pragma once

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace Cleaner {
class Engine;
}
class CleanerRow;
class PillButton;
class QLabel;
class QVBoxLayout;
class SectionHeader;

class CleanerPage : public QWidget
{
    Q_OBJECT

public:
    explicit CleanerPage(QWidget *parent = nullptr);

    int rowCount() const { return m_rowCount; }
    bool scanning() const;

    /// Bytes every measurable target would free right now, and its formatted spelling.
    qint64 reclaimableBytes() const;
    QString reclaimableText() const;

Q_SIGNALS:
    void notice(const QString &text);
    void scanFinished();

private:
    void rescan();
    void rebuild();
    void updateBar();
    void toggleSelectAll();
    void cleanSelected();
    void retranslate();
    QString sizeText(const QString &id) const;
    QString descriptionFor(const QString &id) const;

    Cleaner::Engine *m_engine = nullptr;

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_body = nullptr;
    QLabel *m_summaryLabel = nullptr;
    PillButton *m_rescanButton = nullptr;
    PillButton *m_selectAllButton = nullptr;
    PillButton *m_cleanButton = nullptr;

    QHash<QString, CleanerRow *> m_rows;
    QVector<SectionHeader *> m_headers;
    QHash<QString, qint64> m_bytes;     ///< last measurement; -1 unmeasurable; absent unmeasured
    QHash<QString, qint64> m_files;
    QHash<QString, bool> m_checked;     ///< survives a rebuild (language change) and a rescan
    QStringList m_pendingIds;           ///< rows the clean in flight covers
    QHash<QString, QString> m_results;  ///< result line per row, shown until the next scan
    int m_rowCount = 0;
};
