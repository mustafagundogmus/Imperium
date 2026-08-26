#include "i18n.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSettings>

Q_LOGGING_CATEGORY(lcLocale, "tweaker.locale")

namespace Locale {

namespace {

const QVector<Language> Languages = {
    {QStringLiteral("tr"), QStringLiteral("Türkçe"), false},
    {QStringLiteral("en"), QStringLiteral("English"), false},
    {QStringLiteral("de"), QStringLiteral("Deutsch"), false},
    {QStringLiteral("fr"), QStringLiteral("Français"), false},
    {QStringLiteral("es"), QStringLiteral("Español"), false},
    {QStringLiteral("it"), QStringLiteral("Italiano"), false},
    {QStringLiteral("pt"), QStringLiteral("Português"), false},
    {QStringLiteral("pl"), QStringLiteral("Polski"), false},
    {QStringLiteral("ru"), QStringLiteral("Русский"), false},
    {QStringLiteral("ar"), QStringLiteral("العربية"), true},
};

QString g_language = QStringLiteral("tr");
QHash<QString, QHash<QString, QString>> g_table;   // key -> lang -> text
bool g_loaded = false;

bool isKnown(const QString &id)
{
    for (const Language &l : Languages)
        if (l.id == id)
            return true;
    return false;
}

void load()
{
    if (g_loaded)
        return;
    g_loaded = true;

    QFile file(QStringLiteral(":/data/i18n.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(lcLocale) << "i18n.json missing from resources";
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qCCritical(lcLocale) << "i18n.json parse error:" << err.errorString();
        return;
    }

    const QJsonObject root = doc.object();
    for (auto keyIt = root.constBegin(); keyIt != root.constEnd(); ++keyIt) {
        QHash<QString, QString> perLang;
        const QJsonObject entry = keyIt.value().toObject();
        for (auto langIt = entry.constBegin(); langIt != entry.constEnd(); ++langIt)
            perLang.insert(langIt.key(), langIt.value().toString());
        g_table.insert(keyIt.key(), perLang);
    }
}

} // namespace

const QVector<Language> &languages()
{
    return Languages;
}

void init()
{
    load();
    const QString saved = QSettings().value(QStringLiteral("appearance/language")).toString();
    if (isKnown(saved))
        g_language = saved;
}

QString language()
{
    return g_language;
}

void setLanguage(const QString &id)
{
    if (!isKnown(id) || id == g_language)
        return;
    g_language = id;
    QSettings().setValue(QStringLiteral("appearance/language"), id);
    Q_EMIT notifier()->languageChanged();
}

bool isRtl()
{
    for (const Language &l : Languages)
        if (l.id == g_language)
            return l.rtl;
    return false;
}

QString tr(const QString &key)
{
    const auto entryIt = g_table.constFind(key);
    if (entryIt == g_table.constEnd()) {
        qCWarning(lcLocale) << "missing translation key:" << key;
        return key;
    }

    const auto textIt = entryIt->constFind(g_language);
    if (textIt != entryIt->constEnd() && !textIt->isEmpty())
        return *textIt;

    // Turkish is the source language every key is written against first, so it is the
    // one fallback guaranteed to exist and to actually make sense as a sentence.
    const auto trIt = entryIt->constFind(QStringLiteral("tr"));
    if (trIt != entryIt->constEnd())
        return *trIt;

    return key;
}

QString content(const QString &key, const QString &sourceText)
{
    // Turkish is the language catalog.json is written in, so there is nothing to look up.
    if (g_language == QLatin1String("tr"))
        return sourceText;

    const auto entryIt = g_table.constFind(key);
    if (entryIt == g_table.constEnd())
        return sourceText;   // not translated yet — silent, this is expected during rollout

    const auto textIt = entryIt->constFind(g_language);
    if (textIt != entryIt->constEnd() && !textIt->isEmpty())
        return *textIt;

    return sourceText;
}

bool isFirstRunPending()
{
    return !QSettings().value(QStringLiteral("setup/completed"), false).toBool();
}

void markSetupComplete()
{
    QSettings().setValue(QStringLiteral("setup/completed"), true);
}

Notifier *notifier()
{
    static Notifier n;
    return &n;
}

} // namespace Locale
