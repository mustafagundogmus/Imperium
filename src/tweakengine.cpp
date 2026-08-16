#include "tweakengine.h"

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

        if (isDelete(want) || value.data != want)
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
                          const QString &desired)
{
    const Registry::Hive hive = Registry::hiveFromString(entry.hive);
    const Registry::Value before = Registry::read(hive, entry.path, entry.value);

    QJsonObject record;
    record.insert(QStringLiteral("at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    record.insert(QStringLiteral("id"), tweak.id);
    record.insert(QStringLiteral("index"), index);
    record.insert(QStringLiteral("name"), tweak.name);
    record.insert(QStringLiteral("hive"), entry.hive);
    record.insert(QStringLiteral("path"), entry.path);
    record.insert(QStringLiteral("value"), entry.value);
    record.insert(QStringLiteral("existed"), before.exists);
    record.insert(QStringLiteral("keyExisted"), Registry::keyExists(hive, entry.path));
    record.insert(QStringLiteral("previousType"), before.type);
    record.insert(QStringLiteral("previousData"), before.data);
    record.insert(QStringLiteral("desired"), desired);

    QFile file(m_journalPath);
    if (file.open(QIODevice::Append | QIODevice::Text))
        file.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
}

QVector<TweakEngine::Outcome> TweakEngine::apply(const QVector<QPair<const Tweak *, int>> &requests)
{
    QVector<Outcome> outcomes;
    outcomes.reserve(requests.size());

    for (const auto &request : requests) {
        const Tweak *tweak = request.first;
        const int wanted = qBound(0, request.second, int(tweak->options.size()) - 1);

        Outcome outcome;
        outcome.id = tweak->id;

        if (tweak->reg.isEmpty() || tweak->options.isEmpty()) {
            outcome.error = QStringLiteral("kayıt tanımı eksik");
            outcomes.append(outcome);
            continue;
        }

        // The rows for these cannot be operated, but a preset file can still name them,
        // so the refusal lives here as well as in the UI.
        if (!tweak->editable()) {
            outcome.error = tweak->locked ? QStringLiteral("bu hizmet kilitli")
                                          : QStringLiteral("bu Windows sürümünde geçersiz");
            outcomes.append(outcome);
            continue;
        }

        // Putting a tweak back to what Windows ships is the one direction that consults
        // the journal: the catalogue's idea of the default and what this machine actually
        // had before we touched it are not always the same value.
        const bool restoring = wanted == tweak->defaultOption;
        const TweakOption &option = tweak->options.at(wanted);

        // Elevation is decided for the whole tweak: writing half of a multi-value tweak
        // would leave the machine in a state neither side of the switch describes.
        bool needsElevation = false;
        for (const RegistryEntry &entry : tweak->reg)
            needsElevation = needsElevation
                             || Registry::requiresElevation(Registry::hiveFromString(entry.hive));

        if (needsElevation && !isElevated()) {
            outcome.elevationRequired = true;
            outcome.error = QStringLiteral("yönetici yetkisi gerekiyor");
            outcomes.append(outcome);
            continue;
        }

        bool allOk = true;
        for (int i = 0; i < tweak->reg.size(); ++i) {
            const RegistryEntry &entry = tweak->reg.at(i);
            const Registry::Hive hive = Registry::hiveFromString(entry.hive);
            if (hive == Registry::Hive::Invalid) {
                allOk = false;
                if (outcome.error.isEmpty())
                    outcome.error = QStringLiteral("geçersiz kayıt kovanı");
                continue;
            }

            QString type = entry.type;
            QString target = option.data.value(i);

            journal(*tweak, i, entry, target);

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
                    outcome.restoredOriginal = true;
                }
            }

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
            *error = QStringLiteral("geçersiz kayıt kovanı");
        return false;
    }

    if (entry.existed) {
        const QString type = entry.previousType.isEmpty() ? QStringLiteral("SZ")
                                                          : entry.previousType;
        return Registry::write(hive, entry.path, entry.value, type, entry.previousData, error);
    }

    // There was no value here before. Take the key with it only if we made that too.
    if (!entry.keyExisted)
        return Registry::removeKey(hive, entry.path, error);
    return Registry::remove(hive, entry.path, entry.value, error);
}

bool TweakEngine::needsElevation(const QVector<const Tweak *> &tweaks)
{
    for (const Tweak *tweak : tweaks)
        for (const RegistryEntry &entry : tweak->reg)
            if (Registry::requiresElevation(Registry::hiveFromString(entry.hive)))
                return true;
    return false;
}

bool TweakEngine::isElevated()
{
#ifdef Q_OS_WIN
    static const bool elevated = [] {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        TOKEN_ELEVATION elevation{};
        DWORD size = sizeof(elevation);
        const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
        CloseHandle(token);
        return ok && elevation.TokenIsElevated != 0;
    }();
    return elevated;
#else
    return false;
#endif
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
