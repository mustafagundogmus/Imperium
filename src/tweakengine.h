// tweakengine.h — reads and applies the catalogue against the live registry.
//
// Reading happens at startup so every switch shows what the machine actually does.
// Writing happens only when the user presses "Uygula", one value per tweak, and every
// write is journalled first so the previous contents are recoverable even if a tweak's
// documented "off" value is not what this machine had.

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVector>

struct Tweak;

class TweakEngine : public QObject
{
    Q_OBJECT

public:
    explicit TweakEngine(QObject *parent = nullptr);

    /// Which option every tweak in the catalogue is sitting at, keyed by tweak id.
    QHash<QString, int> readAll() const;

    /// The option whose data the machine matches, or the tweak's default when it matches
    /// none — a machine set to something the catalogue never offered reads as untouched
    /// rather than as an arbitrary position.
    int currentOption(const Tweak &tweak) const;

    /// Convenience for the switch case: option 1 is "on".
    bool isApplied(const Tweak &tweak) const;

    struct Outcome
    {
        QString id;
        bool ok = false;
        bool elevationRequired = false;
        bool restoredOriginal = false;   ///< off wrote the journalled value, not the default
        QString error;
    };

    /// Writes each tweak to the requested option index. One outcome per request.
    QVector<Outcome> apply(const QVector<QPair<const Tweak *, int>> &requests);

    /// One line of the journal: a value this app wrote, and what was there before.
    struct JournalEntry
    {
        QDateTime at;
        QString tweakId;
        QString tweakName;
        QString hive;
        QString path;
        QString value;
        bool existed = false;      ///< the value was there before this write
        bool keyExisted = false;
        QString previousType;
        QString previousData;
        QString desired;           ///< what this app wrote
    };

    /// The journal, newest first. \a limit caps how many are returned, 0 for all.
    QVector<JournalEntry> history(int limit = 0) const;

    /// Puts one journal entry back: writes the previous data, or removes the value when
    /// there was none. This is the only way back for a machine whose catalogue entry has
    /// since changed — the journal remembers the value, the catalogue only remembers the
    /// position it meant.
    bool revert(const JournalEntry &entry, QString *error = nullptr);

    /// True when at least one of these tweaks lives outside HKCU.
    static bool needsElevation(const QVector<const Tweak *> &tweaks);

    static bool isElevated();

    /// Restarts the app through the UAC prompt. Returns false if the user declines.
    static bool relaunchElevated();

    QString journalPath() const { return m_journalPath; }

private:
    /// The value a tweak's key held the very first time this app touched it.
    struct Original
    {
        bool existed = false;      ///< the value was there before this app first wrote it
        bool keyExisted = false;   ///< …and so was the key holding it
        QString type;
        QString data;
    };

    void journal(const Tweak &tweak, int index, const struct RegistryEntry &entry,
                 const QString &desired);
    void loadOriginals();

    QString m_journalPath;
    QHash<QString, Original> m_originals;
};
