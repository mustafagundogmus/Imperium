#include "tweakengine.h"
#include "i18n.h"

#include "catalog.h"
#include "registry.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#endif

namespace {

bool isDelete(const QString &data)
{
    return data.compare(Registry::DeleteSentinel, Qt::CaseInsensitive) == 0;
}

bool isDeleteKey(const QString &data)
{
    return data.compare(Registry::DeleteKeySentinel, Qt::CaseInsensitive) == 0;
}

/// Key for one value of one tweak in the originals map.
QString originalKey(const QString &tweakId, int index)
{
    return tweakId + QLatin1Char('#') + QString::number(index);
}

} // namespace

TweakEngine::TweakEngine(QObject *parent)
    : QObject(parent)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_journalPath = dir + QStringLiteral("/registry-journal.jsonl");
    loadOriginals();
}

void TweakEngine::loadOriginals()
{
    QFile file(m_journalPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    // Only the EARLIEST entry per value matters: later ones record this app's own
    // writes, so keeping the first one preserves whatever the machine had originally.
    while (!file.atEnd()) {
        const QJsonObject entry = QJsonDocument::fromJson(file.readLine()).object();
        const QString id = entry.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;
        const QString key = originalKey(id, entry.value(QStringLiteral("index")).toInt());
        if (m_originals.contains(key))
            continue;

        Original original;
        original.existed = entry.value(QStringLiteral("existed")).toBool();
        // Journals written before this field existed carry no opinion; assuming the key
        // was already there is the cautious reading, since it only ever prevents a delete.
        original.keyExisted = entry.value(QStringLiteral("keyExisted")).toBool(true);
        original.type = entry.value(QStringLiteral("previousType")).toString();
        original.data = entry.value(QStringLiteral("previousData")).toString();
        m_originals.insert(key, original);
    }
}

namespace {

/// True when the machine's registry matches \a option for every key the tweak owns — a
/// half-written tweak sits at no position at all.
bool machineMatches(const Tweak &tweak, const TweakOption &option)
{
    for (int i = 0; i < tweak.reg.size(); ++i) {
        const RegistryEntry &entry = tweak.reg.at(i);
        const QString &want = option.data.value(i);

        const Registry::Hive hive = Registry::hiveFromString(entry.hive);
        if (hive == Registry::Hive::Invalid)
            return false;

        // A position expressed as a missing key is read by looking for the key, not for
        // anything inside it.
        if (isDeleteKey(want)) {
            if (Registry::keyExists(hive, entry.path))
                return false;
            continue;
        }

        const Registry::Value value = Registry::read(hive, entry.path, entry.value);

        if (!value.exists) {
            // Absence is a real state: it matches only a position that asks for it.
            if (!isDelete(want))
                return false;
            continue;
        }

        // Binary is compared through the canonical spelling: Windows writes the tail of a
        // startup blob as a timestamp, and the same bytes reach here upper case from one
        // source and lower case from another. Text values keep their case — for those the
        // difference is the setting.
        const bool binary = entry.type.compare(QLatin1String("BINARY"), Qt::CaseInsensitive) == 0;
        const bool same = binary
                              ? Registry::canonicalBinary(value.data)
                                    == Registry::canonicalBinary(want)
                              : value.data == want;
        if (isDelete(want) || !same)
            return false;
    }
    return true;
}

} // namespace

int TweakEngine::currentOption(const Tweak &tweak) const
{
    if (tweak.reg.isEmpty() || tweak.options.isEmpty())
        return tweak.applied ? 1 : 0;   // nothing to read; take the catalogue's claim

    // Later positions win a tie: a switch whose off and on are both "value absent" would
    // otherwise never read as on, and the on state is the one the user asked about.
    for (int i = int(tweak.options.size()) - 1; i >= 0; --i)
        if (machineMatches(tweak, tweak.options.at(i)))
            return i;

    return tweak.defaultOption;
}

bool TweakEngine::isApplied(const Tweak &tweak) const
{
    return currentOption(tweak) == 1;
}

QHash<QString, int> TweakEngine::readAll() const
{
    QHash<QString, int> states;
    for (const Category &category : Catalog::instance().categories())
        for (const Section &section : category.sections)
            for (const Tweak &tweak : section.tweaks)
                states.insert(tweak.id, currentOption(tweak));
    return states;
}

void TweakEngine::journal(const Tweak &tweak, int index, const RegistryEntry &entry,
                          const QString &desired, const Original &before)
{
    QJsonObject record;
    record.insert(QStringLiteral("at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    record.insert(QStringLiteral("id"), tweak.id);
    record.insert(QStringLiteral("index"), index);
    record.insert(QStringLiteral("name"), tweak.name);
    record.insert(QStringLiteral("hive"), entry.hive);
    record.insert(QStringLiteral("path"), entry.path);
    record.insert(QStringLiteral("value"), entry.value);
    record.insert(QStringLiteral("existed"), before.existed);
    record.insert(QStringLiteral("keyExisted"), before.keyExisted);
    record.insert(QStringLiteral("previousType"), before.type);
    record.insert(QStringLiteral("previousData"), before.data);
    record.insert(QStringLiteral("desired"), desired);

    QFile file(m_journalPath);
    if (file.open(QIODevice::Append | QIODevice::Text))
        file.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');

    // The map this reads back from is built once, in loadOriginals(), from the file as it
    // stood at startup — so without this line "put it back the way this machine had it"
    // did nothing at all for any value first touched in the current session, and started
    // working only after a restart. First write wins, which is the rule loadOriginals()
    // applies to the same records. Inserted whether or not the append above succeeded: a
    // full disk should not also cost the session its knowledge of the original.
    const QString key = originalKey(tweak.id, index);
    if (!m_originals.contains(key))
        m_originals.insert(key, before);
}

QVector<TweakEngine::Outcome> TweakEngine::apply(const QVector<QPair<const Tweak *, int>> &requests)
{
    QVector<Outcome> outcomes;
    outcomes.reserve(requests.size());

    for (const auto &request : requests) {
        const Tweak *tweak = request.first;

        Outcome outcome;
        outcome.id = tweak->id;

        // Before the clamp below, not after: qBound requires min <= max, and a tweak with
        // no options makes the max -1.
        if (tweak->reg.isEmpty() || tweak->options.isEmpty()) {
            outcome.error = Locale::tr(QStringLiteral("err.noRegDef"));
            outcomes.append(outcome);
            continue;
        }

        const int wanted = qBound(0, request.second, int(tweak->options.size()) - 1);

        // The rows for these cannot be operated, but a preset file can still name them,
        // so the refusal lives here as well as in the UI.
        if (!tweak->editable()) {
            outcome.error = Locale::tr(tweak->locked ? QStringLiteral("err.serviceLocked")
                                                     : QStringLiteral("err.notOnThisBuild"));
            outcomes.append(outcome);
            continue;
        }

        // Putting a tweak back to what Windows ships is the one direction that consults
        // the journal: the catalogue's idea of the default and what this machine actually
        // had before we touched it are not always the same value.
        const bool restoring = wanted == tweak->defaultOption && !tweak->literal;
        const TweakOption &option = tweak->options.at(wanted);

        // Elevation is decided for the whole tweak: writing half of a multi-value tweak
        // would leave the machine in a state neither side of the switch describes.
        bool needsElevation = false;
        for (const RegistryEntry &entry : tweak->reg)
            needsElevation = needsElevation
                             || Registry::requiresElevation(Registry::hiveFromString(entry.hive));

        if (needsElevation && !isElevated()) {
            outcome.elevationRequired = true;
            outcome.error = Locale::tr(QStringLiteral("err.needAdmin"));
            outcomes.append(outcome);
            continue;
        }

        // Every value this tweak owns, read before a single one of them is written.
        //
        // This used to happen inside the write loop, one entry at a time, which meant
        // entry i was recorded after entries 0…i-1 had already landed. About twenty
        // catalogue tweaks put a DELETE_KEY on their first entry and keep the rest inside
        // that same key — ctx-control is one — so the loop deleted the key and then
        // journalled "nothing was here" for values it had just destroyed. A journal that
        // says the key was empty is a journal that cannot put it back, and loadOriginals()
        // then cached that lie as what the machine originally held.
        QVector<Original> before;
        before.reserve(tweak->reg.size());
        for (const RegistryEntry &entry : tweak->reg) {
            Original state;
            const Registry::Hive hive = Registry::hiveFromString(entry.hive);
            if (hive != Registry::Hive::Invalid) {
                const Registry::Value value = Registry::read(hive, entry.path, entry.value);
                state.existed = value.exists;
                state.type = value.type;
                state.data = value.data;
                state.keyExisted = Registry::keyExists(hive, entry.path);
            }
            before.append(state);
        }

        bool allOk = true;
        for (int i = 0; i < tweak->reg.size(); ++i) {
            const RegistryEntry &entry = tweak->reg.at(i);
            const Registry::Hive hive = Registry::hiveFromString(entry.hive);
            if (hive == Registry::Hive::Invalid) {
                allOk = false;
                if (outcome.error.isEmpty())
                    outcome.error = Locale::tr(QStringLiteral("err.badHive"));
                continue;
            }

            QString type = entry.type;
            QString target = option.data.value(i);

            // Restoring writes what this machine actually had, not the catalogue's idea
            // of the Windows default — those differ whenever something else had already
            // written the value.
            if (restoring) {
                const auto original = m_originals.constFind(originalKey(tweak->id, i));
                if (original != m_originals.cend()) {
                    if (original->existed) {
                        target = original->data;
                        if (!original->type.isEmpty())
                            type = original->type;
                    } else {
                        // The value was not there before we wrote it. Take the key with
                        // it only when the catalogue says the key *is* the switch and the
                        // key was not there either — otherwise an unrelated setting that
                        // happens to live in the same key would go with it.
                        target = (isDeleteKey(option.data.value(i)) && !original->keyExisted)
                                     ? Registry::DeleteKeySentinel
                                     : Registry::DeleteSentinel;
                    }
                }
            }

            // Journalled after `target` has settled, so the record names the value this
            // write actually puts there. Journalling the catalogue's target first meant
            // a restore recorded — and the Log page then offered to undo — a value that
            // was never written.
            journal(*tweak, i, entry, target, before.at(i));

            QString error;
            bool ok;
            if (isDeleteKey(target))
                ok = Registry::removeKey(hive, entry.path, &error);
            else if (isDelete(target))
                ok = Registry::remove(hive, entry.path, entry.value, &error);
            else
                ok = Registry::write(hive, entry.path, entry.value, type, target, &error);
            if (!ok) {
                allOk = false;
                if (outcome.error.isEmpty())
                    outcome.error = error;
            }
        }

        outcome.ok = allOk;
        outcomes.append(outcome);
    }

    return outcomes;
}

QVector<TweakEngine::JournalEntry> TweakEngine::history(int limit) const
{
    QVector<JournalEntry> entries;

    QFile file(m_journalPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return entries;

    while (!file.atEnd()) {
        const QJsonObject record = QJsonDocument::fromJson(file.readLine()).object();
        if (record.isEmpty())
            continue;

        JournalEntry entry;
        entry.at = QDateTime::fromString(record.value(QStringLiteral("at")).toString(), Qt::ISODate);
        entry.tweakId = record.value(QStringLiteral("id")).toString();
        entry.tweakName = record.value(QStringLiteral("name")).toString();
        entry.hive = record.value(QStringLiteral("hive")).toString();
        entry.path = record.value(QStringLiteral("path")).toString();
        entry.value = record.value(QStringLiteral("value")).toString();
        entry.existed = record.value(QStringLiteral("existed")).toBool();
        entry.keyExisted = record.value(QStringLiteral("keyExisted")).toBool(true);
        entry.previousType = record.value(QStringLiteral("previousType")).toString();
        entry.previousData = record.value(QStringLiteral("previousData")).toString();
        entry.desired = record.value(QStringLiteral("desired")).toString();

        if (!entry.tweakId.isEmpty())
            entries.append(entry);
    }

    // Newest first: the thing you want to undo is almost always the last thing you did.
    std::reverse(entries.begin(), entries.end());
    if (limit > 0 && entries.size() > limit)
        entries.resize(limit);
    return entries;
}

bool TweakEngine::revert(const JournalEntry &entry, QString *error)
{
    const Registry::Hive hive = Registry::hiveFromString(entry.hive);
    if (hive == Registry::Hive::Invalid) {
        if (error)
            *error = Locale::tr(QStringLiteral("err.badHive"));
        return false;
    }

    // A line that recorded a whole-key deletion cannot be undone from a single value.
    // DELETE_KEY took the key and everything under it — other values, subkeys, whatever
    // else lived there — and one journal record describes one value. Writing that value
    // back would rebuild a fraction of what was removed and report it as a full undo, so
    // the honest answer is to refuse. The Log page greys these rows out for the same
    // reason; this is the guard behind that.
    if (entry.desired.compare(Registry::DeleteKeySentinel, Qt::CaseInsensitive) == 0) {
        if (error)
            *error = Locale::tr(QStringLiteral("err.cannotRevertKeyDelete"));
        return false;
    }

    if (entry.existed) {
        const QString type = entry.previousType.isEmpty() ? QStringLiteral("SZ")
                                                          : entry.previousType;
        return Registry::write(hive, entry.path, entry.value, type, entry.previousData, error);
    }

    // There was no value here before, so undoing the write means taking the value away.
    if (!Registry::remove(hive, entry.path, entry.value, error))
        return false;

    // …and only then, if we also made the key, dropping it — but only while it is still
    // empty. `keyExisted` is one instant's snapshot and these keys are shared: thirteen
    // catalogue tweaks write into the Windows Update policy key alone, and a domain
    // policy or another tool may have written there since. This used to be an outright
    // RegDeleteTreeW of the key, which took every one of those with it and then reported
    // success. removeEmptyKey() leaves an occupied key alone; an empty policy key left
    // behind is inert, since only DELETE_KEY positions read a key's existence as state.
    if (!entry.keyExisted)
        Registry::removeEmptyKey(hive, entry.path);
    return true;
}

bool TweakEngine::isElevated()
{
    return Registry::isElevated();
}

bool TweakEngine::relaunchElevated()
{
#ifdef Q_OS_WIN
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";   // this is what raises the UAC prompt
    info.lpFile = reinterpret_cast<const wchar_t *>(exe.utf16());
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info))
        return false;   // user declined the prompt, or it failed
    if (info.hProcess)
        CloseHandle(info.hProcess);
    return true;
#else
    return false;
#endif
}
