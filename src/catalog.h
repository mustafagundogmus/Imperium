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

/// One registry value a tweak owns. A tweak may own several: plenty of real tweaks
/// only take effect when two or three values move together.
struct RegistryEntry
{
    QString hive;      // HKLM | HKCU | HKCR
    QString path;      // key path
    QString value;     // value name
    QString type;      // DWORD | QWORD | SZ | EXPAND_SZ | BINARY
    QString on;        // written when the switch is on
    QString off;       // written when it is off; "DELETE" removes the value
};

/// One position of a tweak, carrying the data for every registry entry it owns.
///
/// A switch is a tweak with two of these — off, then on — synthesised from each entry's
/// `off` and `on`. A choice spells them out in the catalogue instead. Everything past the
/// parser works on the index, so the two kinds need no separate machinery.
struct TweakOption
{
    QString label;           ///< shown on the segmented control; empty for a switch
    QVector<QString> data;   ///< one per RegistryEntry, in order

    /// Set instead of \a label by the synthesised tweaks, whose positions are named by
    /// this app rather than by catalog.json. Looking the text up at draw time is what
    /// keeps it following the interface language: the catalogue is built once, in
    /// whatever language the app started in, so a label resolved during construction
    /// stays in that language for the rest of the session.
    QString labelKey;

    /// The label in the interface language. Keyed by the Turkish text rather than by
    /// position: the same word ("Varsayılan", "Asla") recurs across unrelated tweaks, and
    /// one entry per distinct word is both fewer keys and a consistent translation.
    /// Numeric labels from a range tweak ("8", "64 MB") pass straight through.
    QString displayLabel() const;
};

/// How a machine-generated row describes itself.
///
/// A service row says "Spooler · çalışıyor"; a startup row says
/// "Başlangıç klasörü · C:\…\thing.lnk". Neither sentence comes from catalog.json, so
/// neither can go through the catalogue's own translation table — and assembling it at
/// load time froze it in the language the app happened to start in. The pieces are kept
/// apart here and joined at draw time instead.
struct LiveDescription
{
    bool active = false;   ///< false for an ordinary catalogued tweak

    QString leadKey;       ///< i18n key for the first part…
    QString lead;          ///< …or its literal text, when it has no key
    QString detail;        ///< the machine's own words: a command line, a service key
    QString stateKey;      ///< i18n key for a trailing state word, e.g. "svc.running"
    QString note;          ///< a warning, shown behind the "dikkat:" prefix…
    QString noteKey;       ///< …or its i18n key, which is what the service rows use

    /// The assembled sentence, in the interface language.
    QString text() const;
};

struct Tweak
{
    QString id;
    QString name;
    QString desc;
    bool applied = false;   ///< state already committed to the system
    bool on = false;        ///< state the switch starts in (differs → pending)
    QVector<RegistryEntry> reg;

    QVector<TweakOption> options;   ///< always ≥ 2
    int defaultOption = 0;          ///< what Windows ships; "etkin" means anything else
    bool isChoice = false;          ///< drawn as a segmented control, not a switch
    bool isRange = false;           ///< …or as a slider: the positions are a number line

    // Putting a normal tweak back to `defaultOption` writes what this machine held before
    // the app first touched it, not the catalogue's value — the two differ whenever
    // something else got there first. A literal tweak opts out: its positions are the
    // actual bytes, not a stand-in for "the Windows default". Startup entries are the
    // case that needs it. Their default position is "runs at login", and the value the
    // machine held before was the blob that said it does not — restoring that would
    // switch an entry back on by writing the bytes that keep it off.
    bool literal = false;

    // Windows keeps retiring the values it reads. TaskbarSi still writes cleanly on 26200
    // and Windows ignores it completely — so a tweak can declare the builds it means
    // something on, and one that does not apply here is shown greyed out with the reason
    // rather than offered as a switch that quietly does nothing.
    int minBuild = 0;               ///< 0 = no lower bound
    int maxBuild = 0;               ///< 0 = no upper bound
    bool applicable = true;         ///< resolved against this machine at load
    QString requirement;            ///< "Windows 11 22H2 ve öncesi", when it does not apply

    // Some things are writable and still must not be offered: a service the machine
    // cannot start without is one registry value away from an unbootable Windows.
    bool locked = false;
    QString lockReason;

    QString tooltip;                ///< long text that does not belong on a one-line row

    /// Set for services and startup entries; see LiveDescription. When it is active,
    /// displayDesc() builds the row's description from it rather than from `desc`.
    LiveDescription live;

    /// The row may be operated: it applies here and it is not load-bearing.
    bool editable() const { return applicable && !locked; }
    /// Why not, in the voice the row shows it in.
    QString blockReason() const { return locked ? lockReason : requirement; }

    /// Human-readable target, e.g. "HKCU\Software\…\Enabled" (+2 more).
    QString targetSummary() const;

    /// The name and description in the interface language, falling back to the Turkish
    /// text carried in catalog.json when this entry has not been translated yet. Every
    /// place that shows a tweak to a human goes through these rather than reading `name`
    /// and `desc` straight, so the catalogue can be translated a category at a time
    /// without the untranslated half turning into raw lookup keys.
    ///
    /// Machine-generated tweaks (services, startup items) are skipped: their text is
    /// Windows' own and already comes back in the system's language.
    QString displayName() const;
    QString displayDesc() const;
};

struct Section
{
    QString title;
    QString titleKey;   ///< set instead of \a title by the synthesised sections
    QVector<Tweak> tweaks;

    /// Section headings are keyed by their Turkish text rather than by position, so
    /// reordering the catalogue cannot silently repoint a translation at another heading.
    /// A synthesised section carries a titleKey instead, for the same reason
    /// TweakOption::labelKey exists.
    QString displayTitle() const;
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

    /// Turns every Win32 service on this machine into a four-position choice tweak and
    /// files them under the Hizmetler category. Synthesised rather than catalogued: the
    /// list is the machine's, not ours.
    void appendServices();

    /// Every Run entry and Startup shortcut on this machine, as a switch that writes the
    /// same StartupApproved blob Task Manager does.
    void appendStartup();

    /// The category \a id, or nullptr. The mutable counterpart of category() below,
    /// used by the two synthesisers while the catalogue is still being built.
    Category *mutableCategory(const QString &id);

    QVector<Category> m_categories;
    QHash<QString, const Tweak *> m_byId;
    int m_total = 0;
};

/// Walks every tweak in the catalogue.
///
/// The triple-nested category/section/tweak loop was written out eight times across the
/// app — counting applied tweaks, counting incompatible ones, indexing, searching. Each
/// copy is three lines of scaffolding around one line of actual work, and each one is a
/// place to get the nesting subtly wrong.
template <typename F>
void forEachTweak(const Catalog &catalog, F &&fn)
{
    for (const Category &category : catalog.categories())
        for (const Section &section : category.sections)
            for (const Tweak &tweak : section.tweaks)
                fn(tweak);
}
