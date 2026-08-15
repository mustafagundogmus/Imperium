// catalog.h — the tweak catalogue, loaded from :/data/catalog.json.
//
// The eleven categories, their order and their 12×12 icon paths come verbatim from the
// mockup's CATS array. Each category's sections and tweaks are authored content sized to
// the counts the design shows in the sidebar (24 / 18 / 31 / 12 / 46 / 9 / 22 / 14 / 7 / 11).
//
// `reg` describes where a tweak lives in the registry. It is carried through as data only —
// nothing in this build reads or writes the registry for tweaks. See docs/ARCHITECTURE.md.

#pragma once

#include <QHash>
#include <QString>
#include <QVector>

struct RegistrySpec
{
    QString hive;      // HKLM | HKCU | HKCR | NONE
    QString path;      // key path, or the service name when type == "SERVICE"
    QString value;     // value name
    QString type;      // DWORD | SZ | EXPAND_SZ | QWORD | SERVICE | COMMAND | NONE
    QString on;        // value written when the tweak is enabled
    QString off;       // value that restores the Windows default
};

struct Tweak
{
    QString id;
    QString name;
    QString desc;
    bool applied = false;   ///< state already committed to the system
    bool on = false;        ///< state the switch starts in (differs → pending)
    RegistrySpec reg;
};

struct Section
{
    QString title;
    QVector<Tweak> tweaks;
};

struct Category
{
    QString id;
    QString name;
    QString icon;           ///< SVG path data, 12×12 viewBox
    QVector<Section> sections;

    bool isOverview() const { return id == QLatin1String("ov"); }
    int tweakCount() const;
};

class Catalog
{
public:
    /// Parses :/data/catalog.json. Fatal-logs and returns an empty catalogue on failure.
    static const Catalog &instance();

    const QVector<Category> &categories() const { return m_categories; }
    const Category *category(const QString &id) const;
    const Tweak *tweak(const QString &id) const;

    int totalTweaks() const { return m_total; }
    int tweakCategoryCount() const;   ///< categories that actually hold tweaks

private:
    Catalog();
    void load();

    QVector<Category> m_categories;
    QHash<QString, const Tweak *> m_byId;
    int m_total = 0;
};
