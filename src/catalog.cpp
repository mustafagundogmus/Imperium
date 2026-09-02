#include "catalog.h"
#include "i18n.h"
#include "progress.h"
#include "services.h"
#include "startup.h"
#include "sysinfo.h"
#include "tasks.h"

#include <QCoreApplication>
#include <QDir>
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
    // A scheduled task is named by its path alone; there is no value to speak of.
    if (Tasks::isTaskEntry(first.hive))
        return first.path;
    // An empty value name is the key's default value, not a missing field.
    const QString value = first.value.isEmpty() ? Locale::tr(QStringLiteral("tweak.defaultValue")) : first.value;
    QString text = first.hive + QLatin1String("\\") + first.path + QLatin1String("\\") + value;
    if (reg.size() > 1)
        text += QStringLiteral("  (+%1)").arg(reg.size() - 1);
    return text;
}

QString TweakOption::displayLabel() const
{
    // A synthesised position names itself by key, so it follows the interface language
    // instead of freezing in whichever one the catalogue was built in.
    if (!labelKey.isEmpty())
        return Locale::tr(labelKey);
    if (label.isEmpty())
        return label;
    // Every label goes through the lookup, including the ones that start with a digit.
    // Those used to return early, on the reasoning that a range's positions are numbers
    // with a unit and there is nothing in "512 MB" to translate. That holds for a range,
    // and for a choice whose positions are "22H2" or "50 MB" — but not for one whose
    // positions are "5 saniye" and "1 dakika", which the early return left in Turkish in
    // all ten languages with nothing reporting it. content() falls back to the label when
    // there is no key, so the untranslatable ones still cost exactly one lookup and come
    // back unchanged; only the labels somebody has actually written a key for move.
    return Locale::content(QStringLiteral("opt.") + label, label);
}

QString LiveDescription::text() const
{
    QStringList parts;
    if (!leadKey.isEmpty())
        parts << Locale::tr(leadKey);
    else if (!lead.isEmpty())
        parts << lead;
    if (!detail.isEmpty())
        parts << detail;
    if (!stateKey.isEmpty())
        parts << Locale::tr(stateKey);
    if (!noteKey.isEmpty() || !note.isEmpty())
        parts << Locale::tr(QStringLiteral("svc.riskPrefix"))
                     + (noteKey.isEmpty() ? note : Locale::tr(noteKey));
    return parts.join(QStringLiteral(" · "));
}

/// True for a row this app synthesised from the machine rather than read from
/// catalog.json. Their names are Windows' own, already in the system's language, so
/// there is nothing here for this app's translation table to improve on.
///
/// The startup half used to be spelled "boot-", which is the *category* id — no tweak id
/// has ever started with it, so every startup row was taking the catalogue lookup path
/// and only the fallback saved it from showing a raw key.
static bool isSynthesised(const QString &id)
{
    return id.startsWith(QLatin1String("svc-")) || id.startsWith(QLatin1String("startup-"))
           || id.startsWith(QLatin1String("task-"));
}

QString Tweak::displayName() const
{
    if (isSynthesised(id))
        return name;
    return Locale::content(QStringLiteral("tweak.") + id + QStringLiteral(".name"), name);
}

QString Tweak::displayDesc() const
{
    if (live.active)
        return live.text();
    if (isSynthesised(id))
        return desc;
    return Locale::content(QStringLiteral("tweak.") + id + QStringLiteral(".desc"), desc);
}

QString Section::displayTitle() const
{
    if (!titleKey.isEmpty())
        return Locale::tr(titleKey);
    return Locale::content(QStringLiteral("section.") + title, title);
}

int Category::tweakCount() const
{
    int n = 0;
    for (const Section &s : sections)
        n += s.tweaks.size();
    return n;
}

namespace {

/// Windows names its builds; the numbers alone tell a user nothing.
QString buildLabel(int build)
{
    if (build >= 26100) return QStringLiteral("Windows 11 24H2");
    if (build >= 22631) return QStringLiteral("Windows 11 23H2");
    if (build >= 22621) return QStringLiteral("Windows 11 22H2");
    if (build >= 22000) return QStringLiteral("Windows 11");
    if (build >= 19041) return QStringLiteral("Windows 10 2004");
    return Locale::tr(QStringLiteral("tweak.req.build")).arg(build);
}

/// Fills in the things a catalogue file cannot know when it is written.
///
/// %ARBITRIUM% is where this executable is sitting. A shell verb that has to run with an
/// elevated token calls Arbitrium rather than a console tool, because the verb itself
/// cannot elevate — and a portable build has no fixed install path to hard-code. Since
/// the resolved path is also what the state is read back against, moving the exe makes
/// those tweaks read as off, which is the honest answer: the menu entry is pointing at
/// somewhere the program no longer is, and applying again repairs it.
QString resolvePlaceholders(const QString &data)
{
    if (!data.contains(QLatin1String("%ARBITRIUM%")))
        return data;
    return QString(data).replace(QLatin1String("%ARBITRIUM%"),
                                 QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

/// Decides whether a tweak means anything on this machine, and if not, says why in the
/// same voice the rest of the catalogue is written in.
void resolveApplicability(Tweak &t)
{
    const int build = SysInfo::buildNumber();
    if (build <= 0)
        return;   // unknown machine: assume everything applies rather than hide it all

    if (t.minBuild > 0 && build < t.minBuild) {
        t.applicable = false;
        t.requirement = Locale::tr(QStringLiteral("tweak.req.min")).arg(buildLabel(t.minBuild));
    } else if (t.maxBuild > 0 && build > t.maxBuild) {
        t.applicable = false;
        t.requirement = Locale::tr(QStringLiteral("tweak.req.max"))
                            .arg(buildLabel(t.maxBuild));
    }
}

} // namespace

const Catalog &Catalog::instance()
{
    static Catalog c;
    return c;
}

Catalog::Catalog()
{
    load();
}

Category *Catalog::mutableCategory(const QString &id)
{
    for (Category &c : m_categories)
        if (c.id == id)
            return &c;
    return nullptr;
}

void Catalog::appendServices()
{
    Category *category = mutableCategory(QStringLiteral("svc"));
    if (!category)
        return;

    const QVector<Services::Info> services = Services::enumerate();
    if (services.isEmpty())
        return;

    Section section;
    section.titleKey = QStringLiteral("svc.sectionTitle");
    section.tweaks.reserve(services.size());

    for (const Services::Info &service : services) {
        const QString path = QStringLiteral("SYSTEM\\CurrentControlSet\\Services\\") + service.key;

        Tweak t;
        t.id = QStringLiteral("svc-") + service.key;
        t.name = service.displayName;
        // Windows' own description, in the system language; the key name is a poor but
        // honest substitute for the services that ship without one.
        // The row says what the service *is* — its key and whether it is running — and
        // keeps Windows' paragraph for the tooltip. A sentence elided at the row's edge
        // tells you less than "Spooler · çalışıyor" does.
        // Assembled at draw time rather than here: the catalogue is built once, so a
        // sentence joined now would keep the language the app started in for the rest of
        // the session. "durdu" and the "dikkat:" prefix were also the only two words in
        // it that had never been given a key at all.
        t.live.active = true;
        t.live.lead = service.key;
        t.live.stateKey = service.running ? QStringLiteral("svc.running")
                                          : QStringLiteral("svc.stopped");
        // A warning belongs on the row, not only in a tooltip nobody hovers.
        t.live.noteKey = service.riskNoteKey;
        t.tooltip = service.description;
        t.locked = service.locked;
        t.lockReason = service.lockReason;
        t.isChoice = true;
        t.reg = {
            {QStringLiteral("HKLM"), path, QStringLiteral("Start"), QStringLiteral("DWORD"), {}, {}},
            {QStringLiteral("HKLM"), path, QStringLiteral("DelayedAutostart"), QStringLiteral("DWORD"), {}, {}},
        };
        // Named by key, not by resolved text — see TweakOption::labelKey.
        t.options = {
            {{}, {QStringLiteral("2"), QStringLiteral("0")}, QStringLiteral("svc.opt.auto")},
            {{}, {QStringLiteral("2"), QStringLiteral("1")}, QStringLiteral("svc.opt.delayed")},
            {{}, {QStringLiteral("3"), QStringLiteral("0")}, QStringLiteral("svc.opt.manual")},
            {{}, {QStringLiteral("4"), QStringLiteral("0")}, QStringLiteral("svc.opt.disabled")},
        };

        // There is no universal default for a service, so "how this machine was found"
        // is the baseline: "etkin" then means a service you have moved yourself.
        if (service.start == 2)
            t.defaultOption = service.delayed ? 1 : 0;
        else
            t.defaultOption = service.start == 3 ? 2 : 3;

        // …which is exactly why the baseline is not a stand-in the journal could improve
        // on — see Tweak::literal. Every position here is already the literal Start and
        // DelayedAutostart pair. Without this, picking the position the machine happened
        // to be found in makes apply() consult the journal instead and write the Start an
        // earlier session recorded, so the service stays where it was while the row and
        // the applied count both say it moved.
        t.literal = true;

        section.tweaks.append(t);
        ++m_total;
    }

    category->sections.append(section);
}

void Catalog::appendStartup()
{
    Category *category = mutableCategory(QStringLiteral("boot"));
    if (!category)
        return;

    const QVector<Startup::Entry> entries = Startup::enumerate();
    if (entries.isEmpty())
        return;

    Section section;
    section.titleKey = QStringLiteral("boot.sectionTitle");
    section.tweaks.reserve(entries.size());

    for (const Startup::Entry &entry : entries) {
        Tweak t;
        // Hive and approval key are part of the id because the name on its own is not
        // unique: OneDrive, the audio tray and half the vendor updaters exist under both
        // HKCU and HKLM, and two rows sharing an id share a switch — flipping one would
        // move the other's control and write neither.
        t.id = QStringLiteral("startup-%1-%2-%3")
                   .arg(entry.approvedHive,
                        entry.approvedPath.section(QLatin1Char('\\'), -1),
                        entry.approvedValue);
        t.name = entry.name;
        // The two folder sources are words this app chose, so they follow the interface
        // language; the hive names are not.
        t.live.active = true;
        t.live.leadKey = entry.sourceKey;
        t.live.lead = entry.source;
        t.live.detail = entry.command;
        t.tooltip = entry.command;
        t.reg = {
            {entry.approvedHive, entry.approvedPath, entry.approvedValue,
             QStringLiteral("BINARY"), {}, {}},
        };

        // The position that is true right now carries the blob that is actually there,
        // because Windows stamps a timestamp into a disabled one and no fixed string
        // would match it. The other position gets the canonical blob.
        const QString enabled = entry.enabled && !entry.currentBlob.isEmpty()
                                    ? entry.currentBlob : Startup::enabledBlob();
        const QString disabled = !entry.enabled && !entry.currentBlob.isEmpty()
                                     ? entry.currentBlob : Startup::disabledBlob();
        t.options = {{{}, {disabled}, QStringLiteral("boot.opt.off")},
                     {{}, {enabled}, QStringLiteral("boot.opt.on")}};

        // Windows runs a startup entry unless something says otherwise, so "açık" is the
        // default position and a disabled entry is the changed one.
        t.defaultOption = 1;

        // …but "put it back to the default" here means "write the enabled blob", not
        // "write whatever was there before we touched it" — see Tweak::literal.
        t.literal = true;

        section.tweaks.append(t);
        ++m_total;
    }

    category->sections.append(section);
}

void Catalog::appendTasks()
{
    Category *category = mutableCategory(QStringLiteral("task"));
    if (!category)
        return;

    const QVector<Tasks::Info> tasks = Tasks::enumerate();
    if (tasks.isEmpty())
        return;

    // One section per folder, in the order enumerate() sorted them: the folder is the
    // one thing that tells "Microsoft Compatibility Appraiser" and "StartupAppTask" apart
    // from the fifty other tasks with names like that, and the tree is how Task Scheduler
    // itself shows them. The root's few tasks get a named heading; every other heading is
    // the folder path with its separators spelled as breadcrumbs, which is Windows' own
    // text and needs no translation.
    QString openFolder;
    Section section;
    auto flush = [&] {
        if (!section.tweaks.isEmpty())
            category->sections.append(section);
        section = Section();
    };

    for (const Tasks::Info &task : tasks) {
        // openFolder starts null and a task's folder never is, so the first task opens
        // the first section without a special case.
        if (task.folder != openFolder) {
            flush();
            openFolder = task.folder;
            if (task.folder == QLatin1String("\\")) {
                section.titleKey = QStringLiteral("task.rootFolder");
            } else {
                QString title = task.folder;
                if (title.startsWith(QLatin1Char('\\')))
                    title.remove(0, 1);
                section.title = title.replace(QLatin1Char('\\'), QStringLiteral(" \u203a "));
            }
        }

        Tweak t;
        t.id = Tasks::idFor(task.path);
        t.name = task.name;
        // Folder is the section, so the row says what the scheduler says about the task:
        // whether it is hidden, and what state it is in right now.
        t.live.active = true;
        if (task.hidden)
            t.live.leadKey = QStringLiteral("task.hidden");
        switch (task.state) {
        case 1: t.live.stateKey = QStringLiteral("task.state.disabled"); break;
        case 2: t.live.stateKey = QStringLiteral("task.state.queued"); break;
        case 3: t.live.stateKey = QStringLiteral("task.state.ready"); break;
        case 4: t.live.stateKey = QStringLiteral("task.state.running"); break;
        default: break;
        }
        t.live.noteKey = task.riskNoteKey;
        t.tooltip = task.description;
        t.locked = task.locked;
        t.lockReason = task.lockReason;
        t.reg = {{Tasks::Hive, task.path, {}, Tasks::Hive, {}, {}}};
        t.options = {{{}, {QStringLiteral("0")}, QStringLiteral("task.opt.disabled")},
                     {{}, {QStringLiteral("1")}, QStringLiteral("task.opt.enabled")}};

        // As found, like a service: some of Windows' own tasks ship disabled, so there is
        // no universal default to measure against, and "etkin" would otherwise count
        // every task on the machine. Literal for the same reason services are.
        t.defaultOption = task.enabled ? 1 : 0;
        t.literal = true;

        section.tweaks.append(t);
        ++m_total;
    }
    flush();
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
                    entry.on    = resolvePlaceholders(eo.value(QStringLiteral("on")).toString());
                    entry.off   = resolvePlaceholders(eo.value(QStringLiteral("off")).toString());
                    if (!entry.hive.isEmpty() && !entry.path.isEmpty())
                        t.reg.append(entry);
                }

                // A catalogue entry either lists its positions or is a switch, in which
                // case the two positions are the off and on data already on each key.
                const QJsonArray options = to.value(QStringLiteral("options")).toArray();
                if (options.size() >= 2) {
                    t.isChoice = true;
                    for (const QJsonValue &ov : options) {
                        const QJsonObject oo = ov.toObject();
                        TweakOption option;
                        option.label = oo.value(QStringLiteral("label")).toString();

                        // "data" is one value per registry entry, or a bare string when
                        // the tweak owns a single key.
                        const QJsonValue data = oo.value(QStringLiteral("data"));
                        if (data.isArray()) {
                            const QJsonArray items = data.toArray();
                            for (const QJsonValue &iv : items)
                                option.data.append(iv.toString());
                        } else {
                            option.data.append(data.toString());
                        }
                        option.data.resize(t.reg.size());
                        t.options.append(option);
                    }
                    t.defaultOption = qBound(0, to.value(QStringLiteral("default")).toInt(0),
                                             int(t.options.size()) - 1);
                } else if (to.contains(QStringLiteral("range"))) {
                    // A range is a choice whose positions are generated rather than
                    // listed: min to max in steps, so everything downstream still sees
                    // an index and only the control knows it is a number line.
                    const QJsonObject range = to.value(QStringLiteral("range")).toObject();
                    const int from = range.value(QStringLiteral("min")).toInt();
                    const int to_ = range.value(QStringLiteral("max")).toInt();
                    const int step = qMax(1, range.value(QStringLiteral("step")).toInt(1));
                    const QString unit = range.value(QStringLiteral("unit")).toString();
                    const int fallback = range.value(QStringLiteral("default")).toInt(from);

                    t.isRange = true;
                    bool matched = false;
                    for (int v = from; v <= to_; v += step) {
                        TweakOption option;
                        option.label = unit.isEmpty() ? QString::number(v)
                                                      : QStringLiteral("%1 %2").arg(v).arg(unit);
                        option.data.append(QString::number(v));
                        option.data.resize(t.reg.size());
                        for (int i = 1; i < t.reg.size(); ++i)
                            option.data[i] = QString::number(v);
                        t.options.append(option);
                        if (v == fallback) {
                            t.defaultOption = int(t.options.size()) - 1;
                            matched = true;
                        }
                    }

                    // A declared default that is not on the min/step grid never matched, and
                    // defaultOption silently kept its struct initialiser of 0 — which the rest
                    // of the app reads as "this is what Windows ships". Snapping to the nearest
                    // generated stop is the smallest honest answer; the alternative is a range
                    // whose "default" position is whatever the minimum happens to be.
                    if (!matched && !t.options.isEmpty()) {
                        t.defaultOption = qBound(0, qRound(double(fallback - from) / step),
                                                 int(t.options.size()) - 1);
                        qWarning("catalog: %s declares a range default of %d, which is not on the "
                                 "%d/%d grid; snapping to %d",
                                 qUtf8Printable(t.id), fallback, from, step,
                                 from + t.defaultOption * step);
                    }
                } else {
                    TweakOption off;
                    TweakOption on;
                    for (const RegistryEntry &entry : std::as_const(t.reg)) {
                        off.data.append(entry.off);
                        on.data.append(entry.on);
                    }
                    t.options = {off, on};
                    t.defaultOption = 0;
                }

                t.minBuild = to.value(QStringLiteral("minBuild")).toInt(0);
                t.maxBuild = to.value(QStringLiteral("maxBuild")).toInt(0);
                resolveApplicability(t);

                sec.tweaks.append(t);
                ++m_total;
            }
            cat.sections.append(sec);
        }
        m_categories.append(cat);
    }

    // Named separately from the catalogue JSON above it. This is the stage that walks the
    // service control manager and every Run key on the machine, and it is the one most
    // likely to be slow on somebody else's computer — so when a startup stalls, the card
    // should be able to say it was here rather than "loading".
    Splash::report(QStringLiteral("splash.stage.services"));
    appendServices();
    appendStartup();
    // Its own stage for the same reason the service scan has one: the scheduler holds a
    // few hundred tasks and each one is a COM call, so on a slow machine this is where
    // the card should be able to say it is.
    Splash::report(QStringLiteral("splash.stage.tasks"));
    appendTasks();

    // Index after the vectors have stopped growing so the pointers stay valid.
    forEachTweak(*this, [this](const Tweak &t) { m_byId.insert(t.id, &t); });

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
