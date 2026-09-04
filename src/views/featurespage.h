// featurespage.h — the Özellikler screen: WinUtil's Features section.
//
// Nine rows read out of WinUtil's feature.json, each scanned against the machine first so
// it can say whether it is already on, a checkbox per row and one Install button for the
// ticked ones — Invoke-WPFFeatureInstall, row by row, with the exact lines shown before
// they run. A DISM row that is on also offers to be turned off, which WinUtil does not do.
// Every run ends in a rescan, so what the rows say is what DISM says.

#pragma once

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "../features.h"

class FeatureRow;
class PillButton;
class QLabel;
class QPlainTextEdit;
class QVBoxLayout;
class SectionHeader;

class FeaturesPage : public QWidget
{
    Q_OBJECT

public:
    explicit FeaturesPage(QWidget *parent = nullptr);

    int rowCount() const;
    int enabledCount() const;
    bool scanning() const;
    bool busy() const;
    QString subtitle() const;

Q_SIGNALS:
    void notice(const QString &text);
    void scanFinished();
    void stateChanged();

private:
    void rescan();
    void rebuild();
    void retranslate();
    void updateBar();
    void toggleSelectAll();
    void installSelected();
    void disableOne(const QString &key);
    QVector<Features::Entry> checkedEntries() const;

    void onStarted(Features::Runner::Job job, int total);
    void onProgress(int done, int total, const QString &key, bool finished, bool ok);
    void onLine(const QString &text);
    void onFinished(Features::Runner::Job job, bool ok, int failures);
    void onScanned(const Features::Machine &machine);

    Features::Runner *m_runner = nullptr;
    Features::Machine m_machine;

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_body = nullptr;
    QLabel *m_summaryLabel = nullptr;
    PillButton *m_rescanButton = nullptr;
    PillButton *m_selectAllButton = nullptr;
    PillButton *m_installButton = nullptr;
    PillButton *m_logButton = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_progressLabel = nullptr;

    QVector<SectionHeader *> m_headers;
    QHash<QString, FeatureRow *> m_rows;
    QSet<QString> m_checked;        ///< survives a rebuild and a rescan
    QStringList m_pendingKeys;      ///< rows the run in flight covers
    QHash<QString, QString> m_results;   ///< result line per row, until the next scan
    QString m_progressText;
    bool m_scanning = true;
};
