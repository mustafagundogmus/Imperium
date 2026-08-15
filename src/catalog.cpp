#include "catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCatalog, "tweaker.catalog")

QString Tweak::targetSummary() const
{
    if (reg.isEmpty())
        return {};
    const RegistryEntry &first = reg.first();
    QString text = first.hive + QLatin1String("\\") + first.path + QLatin1String("\\") + first.value;
    if (reg.size() > 1)
        text += QStringLiteral("  (+%1)").arg(reg.size() - 1);
    return text;
}

int Category::tweakCount() const
{
    int n = 0;
    for (const Section &s : sections)
        n += s.tweaks.size();
    return n;
}

const Catalog &Catalog::instance()
{
    static Catalog c;
    return c;
}

Catalog::Catalog()
{
    load();
}

void Catalog::load()
{
    QFile f(QStringLiteral(":/data/catalog.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qCCritical(lcCatalog) << "catalog.json missing from resources";
        return;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qCCritical(lcCatalog) << "catalog.json parse error:" << err.errorString();
        return;
    }

    const QJsonArray cats = doc.object().value(QStringLiteral("categories")).toArray();
    m_categories.reserve(cats.size());

    for (const QJsonValue &cv : cats) {
        const QJsonObject co = cv.toObject();
        Category cat;
        cat.id = co.value(QStringLiteral("id")).toString();
        cat.name = co.value(QStringLiteral("name")).toString();
        cat.icon = co.value(QStringLiteral("icon")).toString();

        const QJsonArray secs = co.value(QStringLiteral("sections")).toArray();
        cat.sections.reserve(secs.size());
        for (const QJsonValue &sv : secs) {
            const QJsonObject so = sv.toObject();
            Section sec;
            sec.title = so.value(QStringLiteral("title")).toString();

            const QJsonArray tws = so.value(QStringLiteral("tweaks")).toArray();
            sec.tweaks.reserve(tws.size());
            for (const QJsonValue &tv : tws) {
                const QJsonObject to = tv.toObject();
                Tweak t;
                t.id = to.value(QStringLiteral("id")).toString();
                t.name = to.value(QStringLiteral("name")).toString();
                t.desc = to.value(QStringLiteral("desc")).toString();
                t.applied = to.value(QStringLiteral("applied")).toBool();
                t.on = to.value(QStringLiteral("on")).toBool(t.applied);

                const QJsonArray entries = to.value(QStringLiteral("reg")).toArray();
                t.reg.reserve(entries.size());
                for (const QJsonValue &ev : entries) {
                    const QJsonObject eo = ev.toObject();
                    RegistryEntry entry;
                    entry.hive  = eo.value(QStringLiteral("hive")).toString();
                    entry.path  = eo.value(QStringLiteral("path")).toString();
                    entry.value = eo.value(QStringLiteral("value")).toString();
                    entry.type  = eo.value(QStringLiteral("type")).toString();
                    entry.on    = eo.value(QStringLiteral("on")).toString();
                    entry.off   = eo.value(QStringLiteral("off")).toString();
                    if (!entry.hive.isEmpty() && !entry.path.isEmpty())
                        t.reg.append(entry);
                }
                t.source = to.value(QStringLiteral("source")).toString();

                sec.tweaks.append(t);
                ++m_total;
            }
            cat.sections.append(sec);
        }
        m_categories.append(cat);
    }

    // Index after the vectors have stopped growing so the pointers stay valid.
    for (const Category &c : std::as_const(m_categories))
        for (const Section &s : c.sections)
            for (const Tweak &t : s.tweaks)
                m_byId.insert(t.id, &t);

    qCInfo(lcCatalog) << "loaded" << m_categories.size() << "categories," << m_total << "tweaks";
}

const Category *Catalog::category(const QString &id) const
{
    for (const Category &c : m_categories)
        if (c.id == id)
            return &c;
    return nullptr;
}

const Tweak *Catalog::tweak(const QString &id) const
{
    return m_byId.value(id, nullptr);
}

int Catalog::tweakCategoryCount() const
{
    int n = 0;
    for (const Category &c : m_categories)
        if (!c.sections.isEmpty())
            ++n;
    return n;
}
