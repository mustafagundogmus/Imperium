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
        original.type = entry.value(QStringLiteral("previousType")).toString();
        original.data = entry.value(QStringLiteral("previousData")).toString();
        m_originals.insert(key, original);
    }
}

bool TweakEngine::isApplied(const Tweak &tweak) const
{
    if (tweak.reg.isEmpty())
        return tweak.applied;   // nothing to read; fall back to the catalogue's claim

    // A tweak that owns several values is only "applied" when every one of them is at
    // its on state — a half-written tweak is not in effect.
    for (const RegistryEntry &entry : tweak.reg) {
        const Registry::Hive hive = Registry::hiveFromString(entry.hive);
        if (hive == Registry::Hive::Invalid)
            return false;

        const Registry::Value value = Registry::read(hive, entry.path, entry.value);

        if (!value.exists) {
            // Absence is a real state: it only counts as applied when the on state is
            // itself expressed as "delete this value".
            if (!isDelete(entry.on))
                return false;
            continue;
        }

        if (isDelete(entry.on) || value.data != entry.on)
            return false;
    }
    return true;
}

QHash<QString, bool> TweakEngine::readAll() const
{
    QHash<QString, bool> states;
    for (const Category &category : Catalog::instance().categories())
        for (const Section &section : category.sections)
            for (const Tweak &tweak : section.tweaks)
                states.insert(tweak.id, isApplied(tweak));
    return states;
}

void TweakEngine::journal(const Tweak &tweak, int index, const RegistryEntry &entry, bool desired)
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
    record.insert(QStringLiteral("previousType"), before.type);
    record.insert(QStringLiteral("previousData"), before.data);
    record.insert(QStringLiteral("desired"), desired ? entry.on : entry.off);

    QFile file(m_journalPath);
    if (file.open(QIODevice::Append | QIODevice::Text))
        file.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
}

QVector<TweakEngine::Outcome> TweakEngine::apply(const QVector<QPair<const Tweak *, bool>> &requests)
{
    QVector<Outcome> outcomes;
    outcomes.reserve(requests.size());

    for (const auto &request : requests) {
        const Tweak *tweak = request.first;
        const bool desired = request.second;

        Outcome outcome;
        outcome.id = tweak->id;

        if (tweak->reg.isEmpty()) {
            outcome.error = QStringLiteral("kayıt tanımı eksik");
            outcomes.append(outcome);
            continue;
        }

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

            journal(*tweak, i, entry, desired);

            QString type = entry.type;
            QString target = desired ? entry.on : entry.off;

            // Turning a tweak off restores what this machine actually had, not the
            // catalogue's idea of the Windows default — those differ whenever something
            // else had already written the value.
            if (!desired) {
                const auto original = m_originals.constFind(originalKey(tweak->id, i));
                if (original != m_originals.cend()) {
                    target = original->existed ? original->data : Registry::DeleteSentinel;
                    if (original->existed && !original->type.isEmpty())
                        type = original->type;
                    outcome.restoredOriginal = true;
                }
            }

            QString error;
            const bool ok = isDelete(target)
                                ? Registry::remove(hive, entry.path, entry.value, &error)
                                : Registry::write(hive, entry.path, entry.value, type, target, &error);
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
