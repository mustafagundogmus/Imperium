#include "settingslinks.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSettingsLinks, "tweaker.settingslinks")

namespace {

/// Parsed the way ActionCatalog's constructor parses actions.json: field by field, keeping
/// going rather than complaining, so a build with a damaged data file still shows a window.
/// What is silently dropped here is what tools/check-data.py exists to catch before it is.
QVector<SettingsLinks::Group> load()
{
    QVector<SettingsLinks::Group> result;

    QFile file(QStringLiteral(":/data/settings-links.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(lcSettingsLinks) << "settings-links.json missing from resources";
        return result;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qCCritical(lcSettingsLinks) << "settings-links.json parse error:" << error.errorString();
        return result;
    }

    const QJsonArray groups = doc.object().value(QStringLiteral("groups")).toArray();
    result.reserve(groups.size());

    for (const QJsonValue &gv : groups) {
        const QJsonObject go = gv.toObject();
        SettingsLinks::Group group;
        group.id = go.value(QStringLiteral("id")).toString();

        const QJsonArray items = go.value(QStringLiteral("items")).toArray();
        group.links.reserve(items.size());
        for (const QJsonValue &iv : items) {
            const QJsonObject io = iv.toObject();
            SettingsLinks::Link link;
            link.id = io.value(QStringLiteral("id")).toString();
            link.target = io.value(QStringLiteral("target")).toString();

            // A link with no id has no label to draw and a link with no target has nothing
            // to open; either one would be a row that sits there doing nothing.
            if (!link.id.isEmpty() && !link.target.isEmpty())
                group.links.append(link);
        }

        // A group heading with nothing under it is furniture, and a group with no id of its
        // own has no heading to look up in the first place.
        if (!group.id.isEmpty() && !group.links.isEmpty())
            result.append(group);
    }

    return result;
}

} // namespace

const QVector<SettingsLinks::Group> &SettingsLinks::groups()
{
    static const QVector<Group> g = load();
    return g;
}

int SettingsLinks::total()
{
    int n = 0;
    for (const Group &group : groups())
        n += int(group.links.size());
    return n;
}
