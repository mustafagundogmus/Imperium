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

    // Only the EARLIEST entry per tweak matters: later ones record this app's own
    // writes, so keeping the first one preserves whatever the machine had originally.
    while (!file.atEnd()) {
        const QJsonObject entry = QJsonDocument::fromJson(file.readLine()).object();
        const QString id = entry.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || m_originals.contains(id))
            continue;
        Original original;
        original.existed = entry.value(QStringLiteral("existed")).toBool();
        original.type = entry.value(QStringLiteral("previousType")).toString();
        original.data = entry.value(QStringLiteral("previousData")).toString();
        m_originals.insert(id, original);
    }
}

bool TweakEngine::isApplied(const Tweak &tweak) const
{
    const Registry::Hive hive = Registry::hiveFromString(tweak.reg.hive);
    if (hive == Registry::Hive::Invalid || tweak.reg.path.isEmpty())
        return tweak.applied;   // nothing to read; fall back to the catalogue's claim

    const Registry::Value value = Registry::read(hive, tweak.reg.path, tweak.reg.value);

    if (!value.exists) {
        // Absence is a real state: whichever side is expressed as "delete the value" wins.
        if (isDelete(tweak.reg.on))
            return true;
        return false;
    }

    if (!isDelete(tweak.reg.on) && value.data == tweak.reg.on)
        return true;
    // Anything else — the documented default, or a third-party value — counts as not applied.
    return false;
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

void TweakEngine::journal(const Tweak &tweak, bool desired)
{
    const Registry::Hive hive = Registry::hiveFromString(tweak.reg.hive);
    const Registry::Value before = Registry::read(hive, tweak.reg.path, tweak.reg.value);

    QJsonObject entry;
    entry.insert(QStringLiteral("at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    entry.insert(QStringLiteral("id"), tweak.id);
    entry.insert(QStringLiteral("name"), tweak.name);
    entry.insert(QStringLiteral("hive"), tweak.reg.hive);
    entry.insert(QStringLiteral("path"), tweak.reg.path);
    entry.insert(QStringLiteral("value"), tweak.reg.value);
    entry.insert(QStringLiteral("existed"), before.exists);
    entry.insert(QStringLiteral("previousType"), before.type);
    entry.insert(QStringLiteral("previousData"), before.data);
    entry.insert(QStringLiteral("desired"), desired ? tweak.reg.on : tweak.reg.off);

    QFile file(m_journalPath);
    if (file.open(QIODevice::Append | QIODevice::Text))
        file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n');
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

        const Registry::Hive hive = Registry::hiveFromString(tweak->reg.hive);
        if (hive == Registry::Hive::Invalid || tweak->reg.path.isEmpty()) {
            outcome.error = QStringLiteral("kayıt tanımı eksik");
            outcomes.append(outcome);
            continue;
        }

        if (Registry::requiresElevation(hive) && !isElevated()) {
            outcome.elevationRequired = true;
            outcome.error = QStringLiteral("yönetici yetkisi gerekiyor");
            outcomes.append(outcome);
            continue;
        }

        journal(*tweak, desired);

        QString error;
        QString type = tweak->reg.type;
        QString target = desired ? tweak->reg.on : tweak->reg.off;

        // Turning a tweak off restores what this machine actually had, not the
        // catalogue's idea of the Windows default — those differ whenever something
        // else had already written the value.
        if (!desired) {
            const auto original = m_originals.constFind(tweak->id);
            if (original != m_originals.cend()) {
                target = original->existed ? original->data : Registry::DeleteSentinel;
                if (original->existed && !original->type.isEmpty())
                    type = original->type;
                outcome.restoredOriginal = true;
            }
        }

        outcome.ok = isDelete(target)
                         ? Registry::remove(hive, tweak->reg.path, tweak->reg.value, &error)
                         : Registry::write(hive, tweak->reg.path, tweak->reg.value,
                                           type, target, &error);
        outcome.error = error;
        outcomes.append(outcome);
    }

    return outcomes;
}

bool TweakEngine::needsElevation(const QVector<const Tweak *> &tweaks)
{
    for (const Tweak *tweak : tweaks)
        if (Registry::requiresElevation(Registry::hiveFromString(tweak->reg.hive)))
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
