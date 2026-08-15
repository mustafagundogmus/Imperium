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
    const QHash<QString, bool> live = m_engine ? m_engine->readAll() : QHash<QString, bool>();

    for (const Category &c : Catalog::instance().categories()) {
        for (const Section &s : c.sections) {
            for (const Tweak &t : s.tweaks) {
                const bool applied = live.contains(t.id) ? live.value(t.id) : t.applied;
                m_applied.insert(t.id, applied);
                m_on.insert(t.id, applied);
                if (applied)
                    ++m_appliedCount;
            }
        }
    }

    // Pick up anything stashed before an elevated relaunch.
    QSettings settings;
    settings.beginGroup(StashGroup);
    const QStringList stashed = settings.childKeys();
    for (const QString &id : stashed) {
        if (!m_on.contains(id))
            continue;
        m_on[id] = settings.value(id).toBool();
    }
    settings.endGroup();
    if (!stashed.isEmpty()) {
        settings.remove(StashGroup);
        recomputePending();
    }

    const QVariant stamp = settings.value(QStringLiteral("state/lastApplied"));
    if (stamp.isValid())
        m_lastApplied = stamp.toDateTime();
}

void AppState::recomputePending()
{
    m_pending.clear();
    for (auto it = m_on.cbegin(); it != m_on.cend(); ++it)
        if (it.value() != m_applied.value(it.key(), false))
            m_pending.insert(it.key());
}

bool AppState::isOn(const QString &id) const
{
    return m_on.value(id, false);
}

bool AppState::isApplied(const QString &id) const
{
    return m_applied.value(id, false);
}

void AppState::setOn(const QString &id, bool on)
{
    const auto it = m_on.find(id);
    if (it == m_on.end() || *it == on)
        return;
    *it = on;

    const int before = int(m_pending.size());
    if (on != m_applied.value(id, false))
        m_pending.insert(id);
    else
        m_pending.remove(id);

    Q_EMIT tweakToggled(id);
    if (int(m_pending.size()) != before)
        Q_EMIT pendingChanged();
}

void AppState::toggle(const QString &id)
{
    setOn(id, !isOn(id));
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
            if (m_applied.value(t.id, false))
                ++n;
    return n;
}

void AppState::stashPending() const
{
    QSettings settings;
    settings.remove(StashGroup);
    settings.beginGroup(StashGroup);
    for (const QString &id : m_pending)
        settings.setValue(id, m_on.value(id, false));
    settings.endGroup();
}

AppState::ApplyReport AppState::applyPending()
{
    ApplyReport report;
    if (m_pending.isEmpty() || !m_engine)
        return report;

    const Catalog &catalog = Catalog::instance();
    QVector<QPair<const Tweak *, bool>> requests;
    requests.reserve(m_pending.size());
    for (const QString &id : std::as_const(m_pending)) {
        if (const Tweak *tweak = catalog.tweak(id))
            requests.append({tweak, m_on.value(id, false)});
    }

    const QVector<TweakEngine::Outcome> outcomes = m_engine->apply(requests);

    for (const TweakEngine::Outcome &outcome : outcomes) {
        if (outcome.ok) {
            const bool on = m_on.value(outcome.id, false);
            const bool was = m_applied.value(outcome.id, false);
            m_applied[outcome.id] = on;
            if (on && !was)
                ++m_appliedCount;
            else if (!on && was)
                --m_appliedCount;
            m_pending.remove(outcome.id);
            ++report.succeeded;
        } else {
            ++report.failed;
            report.elevationRequired = report.elevationRequired || outcome.elevationRequired;
            if (report.firstError.isEmpty())
                report.firstError = outcome.error;
        }
    }

    if (report.succeeded > 0) {
        m_lastApplied = QDateTime::currentDateTime();
        QSettings().setValue(QStringLiteral("state/lastApplied"), m_lastApplied);
    }

    Q_EMIT pendingChanged();
    Q_EMIT committed(report.succeeded);
    return report;
}

void AppState::revertPending()
{
    if (m_pending.isEmpty())
        return;

    const QSet<QString> reverted = m_pending;
    for (const QString &id : reverted)
        m_on[id] = m_applied.value(id, false);
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
