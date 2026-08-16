// appstate.h — tweak state and view state.
//
// Every tweak is a position: `applied` is the option the registry actually sits at right
// now, read once at startup through TweakEngine, `selected` is where the control sits,
// and `pending` is the set where the two disagree — that count drives the header, the
// status bar and the "Uygula (N)" label.
//
// A switch is a two-position tweak (0 off, 1 on) and a choice is an n-position one, so
// the state here is an index rather than a bool; isOn() is the switch-shaped view of it.
// "Etkin" means a tweak sits at anything other than the position Windows ships.
//
// Pending toggles survive a restart-as-administrator: they are stashed in QSettings
// before the relaunch and picked back up here, so the user does not have to flip the
// same switches twice.

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

struct Category;
class TweakEngine;

enum class Filter
{
    All,        ///< Tümü
    Enabled,    ///< Etkin
    Changed     ///< Değişen
};

class AppState : public QObject
{
    Q_OBJECT

public:
    explicit AppState(TweakEngine *engine, QObject *parent = nullptr);

    // --- positions ----------------------------------------------------------
    int selected(const QString &id) const;
    int appliedOption(const QString &id) const;
    void setSelected(const QString &id, int option);

    /// True when the tweak sits anywhere other than the position Windows ships.
    bool isOn(const QString &id) const;
    bool isApplied(const QString &id) const;
    bool isPending(const QString &id) const { return selected(id) != appliedOption(id); }

    void setOn(const QString &id, bool on);
    void toggle(const QString &id);

    /// Re-reads one tweak from the machine. Used after the journal writes a value back
    /// behind the catalogue's back.
    void refreshFromMachine(const QString &id);

    int pendingCount() const { return int(m_pending.size()); }
    int pendingCount(const Category &c) const;
    int appliedCount() const { return m_appliedCount; }
    int appliedCount(const Category &c) const;

    QList<QString> pendingIds() const { return m_pending.values(); }

    struct ApplyReport
    {
        int succeeded = 0;
        int failed = 0;
        bool elevationRequired = false;
        QString firstError;
    };

    /// Writes every pending change to the registry and folds the results back in.
    ApplyReport applyPending();

    struct StepOutcome
    {
        QString id;
        QString name;
        QString path;    ///< "HKCU\Software\..." — shown while the write happens
        bool ok = false;
        bool elevationRequired = false;
    };

    /// Writes exactly one pending tweak, so an apply can be driven a step at a time.
    StepOutcome applyOne(const QString &id);

    /// Puts every switch back where the system currently stands. Writes nothing.
    void revertPending();

    /// Stashes the pending set so it can be restored after an elevated relaunch.
    void stashPending() const;

    QDateTime lastAppliedAt() const { return m_lastApplied; }

    // --- view state ---------------------------------------------------------
    QString selectedCategory() const { return m_category; }
    void setSelectedCategory(const QString &id);

    Filter filter() const { return m_filter; }
    void setFilter(Filter f);

    QString query() const { return m_query; }
    void setQuery(const QString &q);
    bool searching() const { return !m_query.trimmed().isEmpty(); }

Q_SIGNALS:
    /// The control for \a id should redraw: its position changed under it.
    void tweakToggled(const QString &id);
    void pendingChanged();
    void selectionChanged();
    void filterChanged();
    void queryChanged();
    void committed(int count);

private:
    void recomputePending();

    TweakEngine *m_engine = nullptr;
    QHash<QString, int> m_on;        ///< option index the control sits at
    QHash<QString, int> m_applied;   ///< option index the registry sits at
    QHash<QString, int> m_default;   ///< option index Windows ships, per tweak
    QSet<QString> m_pending;
    int m_appliedCount = 0;

    QString m_category = QStringLiteral("ov");
    Filter m_filter = Filter::All;
    QString m_query;
    QDateTime m_lastApplied;
};
