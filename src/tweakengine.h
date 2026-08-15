// tweakengine.h — reads and applies the catalogue against the live registry.
//
// Reading happens at startup so every switch shows what the machine actually does.
// Writing happens only when the user presses "Uygula", one value per tweak, and every
// write is journalled first so the previous contents are recoverable even if a tweak's
// documented "off" value is not what this machine had.

#pragma once

#include <QHash>
#include <QObject>
#include <QVector>

struct Tweak;

class TweakEngine : public QObject
{
    Q_OBJECT

public:
    explicit TweakEngine(QObject *parent = nullptr);

    /// Current state of every tweak in the catalogue, keyed by tweak id.
    QHash<QString, bool> readAll() const;

    /// State of one tweak. Unknown data that matches neither side reads as off.
    bool isApplied(const Tweak &tweak) const;

    struct Outcome
    {
        QString id;
        bool ok = false;
        bool elevationRequired = false;
        bool restoredOriginal = false;   ///< off wrote the journalled value, not the default
        QString error;
    };

    /// Writes each tweak to \a desired. Returns one outcome per request.
    QVector<Outcome> apply(const QVector<QPair<const Tweak *, bool>> &requests);

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
        bool existed = false;
        QString type;
        QString data;
    };

    void journal(const Tweak &tweak, int index, const struct RegistryEntry &entry, bool desired);
    void loadOriginals();

    QString m_journalPath;
    QHash<QString, Original> m_originals;
};
