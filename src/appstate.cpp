#include "appstate.h"
#include "catalog.h"
#include "tweakengine.h"

#include <QSettings>

namespace {
const QString StashGroup = QStringLiteral("pending");
}

AppState::AppState(TweakEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    // The registry is the source of truth for `applied`; the catalogue's own flag is
    // only a fallback for entries the engine cannot read.
    const QHash<QString, int> live = m_engine ? m_engine->readAll() : QHash<QString, int>();

    forEachTweak(Catalog::instance(), [&](const Tweak &t) {
        const int fallback = t.applied ? 1 : t.defaultOption;
        const int applied = live.contains(t.id) ? live.value(t.id) : fallback;
        m_applied.insert(t.id, applied);
        m_on.insert(t.id, applied);
        m_default.insert(t.id, t.defaultOption);
        if (applied != t.defaultOption)
            ++m_appliedCount;
    });

    // Pick up anything stashed before an elevated relaunch.
    QSettings settings;
    settings.beginGroup(StashGroup);
    const QStringList stashedIds = settings.childKeys();
    for (const QString &id : stashedIds) {
        if (!m_on.contains(id))
            continue;
        // Stashes written before positions existed hold true/false.
        const QVariant value = settings.value(id);
        m_on[id] = value.typeId() == QMetaType::Bool ? (value.toBool() ? 1 : 0)
                                                     : value.toInt();
    }
    settings.endGroup();
    if (!stashedIds.isEmpty()) {
        settings.remove(StashGroup);
        recomputePending();
    }
}

void AppState::noteAppliedMove(const QString &id, int was, int now)
{
    // "Applied" counts the tweaks sitting anywhere other than the position Windows ships,
    // so a move only changes the total when it crosses that position — in one direction
    // or the other. The same six lines were copied into applyOne() and
    // refreshFromMachine(), which is two chances for the arms to drift apart.
    const int fallback = m_default.value(id, 0);
    if (now != fallback && was == fallback)
        ++m_appliedCount;
    else if (now == fallback && was != fallback)
        --m_appliedCount;
}

void AppState::recomputePending()
{
    m_pending.clear();
    for (auto it = m_on.cbegin(); it != m_on.cend(); ++it)
        if (it.value() != m_applied.value(it.key(), 0))
            m_pending.insert(it.key());
}

int AppState::selected(const QString &id) const
{
    return m_on.value(id, 0);
}

int AppState::appliedOption(const QString &id) const
{
    return m_applied.value(id, 0);
}

bool AppState::isOn(const QString &id) const
{
    return selected(id) != m_default.value(id, 0);
}

bool AppState::isApplied(const QString &id) const
{
    return appliedOption(id) != m_default.value(id, 0);
}

void AppState::setSelected(const QString &id, int option)
{
    const auto it = m_on.find(id);
    if (it == m_on.end() || *it == option)
        return;
    *it = option;

    const int before = int(m_pending.size());
    if (option != m_applied.value(id, 0))
        m_pending.insert(id);
    else
        m_pending.remove(id);

    Q_EMIT tweakToggled(id);
    if (int(m_pending.size()) != before)
        Q_EMIT pendingChanged();
}

void AppState::setOn(const QString &id, bool on)
{
    setSelected(id, on ? 1 : 0);
}

void AppState::refreshFromMachine(const QString &id)
{
    const Tweak *tweak = Catalog::instance().tweak(id);
    if (!tweak || !m_engine || !m_applied.contains(id))
        return;

    const int now = m_engine->currentOption(*tweak);
    const int was = m_applied.value(id, 0);
    if (now == was && m_on.value(id, 0) == now)
        return;

    noteAppliedMove(id, was, now);

    m_applied[id] = now;
    m_on[id] = now;
    m_pending.remove(id);

    Q_EMIT tweakToggled(id);
    Q_EMIT pendingChanged();
}

int AppState::pendingCount(const Category &c) const
{
    int n = 0;
    for (const Section &s : c.sections)
        for (const Tweak &t : s.tweaks)
            if (m_pending.contains(t.id))
                ++n;
    return n;
}

int AppState::appliedCount(const Category &c) const
{
    int n = 0;
    for (const Section &s : c.sections)
        for (const Tweak &t : s.tweaks)
            if (m_applied.value(t.id, 0) != t.defaultOption)
                ++n;
    return n;
}

void AppState::stashPending() const
{
    QSettings settings;
    settings.remove(StashGroup);
    settings.beginGroup(StashGroup);
    for (const QString &id : m_pending)
        settings.setValue(id, m_on.value(id, 0));
    settings.endGroup();
}

AppState::StepOutcome AppState::applyOne(const QString &id)
{
    StepOutcome outcome;
    outcome.id = id;

    const Tweak *tweak = Catalog::instance().tweak(id);
    if (!tweak || !m_engine || !m_pending.contains(id))
        return outcome;

    outcome.name = tweak->name;
    outcome.path = tweak->targetSummary();

    const int desired = m_on.value(id, 0);
    const QVector<TweakEngine::Outcome> results = m_engine->apply({{tweak, desired}});
    if (results.isEmpty())
        return outcome;

    const TweakEngine::Outcome &result = results.first();
    outcome.ok = result.ok;
    outcome.elevationRequired = result.elevationRequired;
    outcome.error = result.error;

    if (result.ok) {
        const int was = m_applied.value(id, 0);
        m_applied[id] = desired;
        noteAppliedMove(id, was, desired);
        m_pending.remove(id);

        Q_EMIT pendingChanged();
    }

    return outcome;
}

void AppState::revertPending()
{
    if (m_pending.isEmpty())
        return;

    const QSet<QString> reverted = m_pending;
    for (const QString &id : reverted)
        m_on[id] = m_applied.value(id, 0);
    m_pending.clear();

    for (const QString &id : reverted)
        Q_EMIT tweakToggled(id);
    Q_EMIT pendingChanged();
}

void AppState::setSelectedCategory(const QString &id)
{
    if (m_category == id)
        return;
    m_category = id;
    Q_EMIT selectionChanged();
}

void AppState::setFilter(Filter f)
{
    if (m_filter == f)
        return;
    m_filter = f;
    Q_EMIT filterChanged();
}

void AppState::setQuery(const QString &q)
{
    if (m_query == q)
        return;
    m_query = q;
    Q_EMIT queryChanged();
}
