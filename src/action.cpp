#include "action.h"

#include "i18n.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcActions, "tweaker.actions")

QString Action::displayName() const
{
    return Locale::content(QStringLiteral("action.") + id + QStringLiteral(".name"), name);
}

QString Action::displayDesc() const
{
    return Locale::content(QStringLiteral("action.") + id + QStringLiteral(".desc"), desc);
}

QString Action::displayNote() const
{
    if (note.isEmpty())
        return note;
    return Locale::content(QStringLiteral("action.") + id + QStringLiteral(".note"), note);
}

QString ActionSection::displayTitle() const
{
    return Locale::content(QStringLiteral("section.") + title, title);
}

const ActionCatalog &ActionCatalog::instance()
{
    static ActionCatalog c;
    return c;
}

int ActionCatalog::total() const
{
    int n = 0;
    for (const ActionSection &s : m_sections)
        n += s.actions.size();
    return n;
}

ActionCatalog::ActionCatalog()
{
    QFile file(QStringLiteral(":/data/actions.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(lcActions) << "actions.json missing from resources";
        return;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qCCritical(lcActions) << "actions.json parse error:" << error.errorString();
        return;
    }

    const QJsonArray sections = doc.object().value(QStringLiteral("sections")).toArray();
    m_sections.reserve(sections.size());

    for (const QJsonValue &sv : sections) {
        const QJsonObject so = sv.toObject();
        ActionSection section;
        section.title = so.value(QStringLiteral("title")).toString();

        const QJsonArray actions = so.value(QStringLiteral("actions")).toArray();
        section.actions.reserve(actions.size());
        for (const QJsonValue &av : actions) {
            const QJsonObject ao = av.toObject();
            Action action;
            action.id = ao.value(QStringLiteral("id")).toString();
            action.name = ao.value(QStringLiteral("name")).toString();
            action.desc = ao.value(QStringLiteral("desc")).toString();
            action.reversible = ao.value(QStringLiteral("reversible")).toBool();
            action.note = ao.value(QStringLiteral("note")).toString();

            const QJsonArray run = ao.value(QStringLiteral("run")).toArray();
            for (const QJsonValue &line : run)
                action.run << line.toString();

            if (!action.id.isEmpty() && !action.run.isEmpty())
                section.actions.append(action);
        }

        if (!section.actions.isEmpty())
            m_sections.append(section);
    }
}
