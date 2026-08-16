#include "migration.h"

#include "registry.h"

#include <QSettings>

namespace {

const QString KeyLevel = QStringLiteral("app/migration");

/// The context-menu rework of 0.9.3.
///
/// Take ownership moved to the verb named `runas`, because that is the one name the
/// shell elevates before running — under the old name the command ran with a plain
/// token and could not touch a file TrustedInstaller owned. The old verb keys are still
/// sitting in the shell, showing a second entry that does not work.
///
/// The power plan verb kept its key but became a cascading menu, and a verb cannot both
/// carry a command and open a submenu; the command left over from the old shape has to
/// go for the submenu to appear.
bool contextMenuRework()
{
    bool ok = true;

    for (const QString &path : {QStringLiteral("*\\shell\\ArbitriumTakeOwnership"),
                                QStringLiteral("Directory\\shell\\ArbitriumTakeOwnership"),
                                QStringLiteral("DesktopBackground\\Shell\\ArbitriumPowerPlan\\command")}) {
        ok = Registry::removeKey(Registry::Hive::HKCR, path) && ok;
    }

    // The shield badge these carried was decoration: the entries open consoles that
    // elevate themselves, and the badge on a verb that does not elevate is a lie.
    for (const QString &verb : {QStringLiteral("ArbitriumServices"),
                                QStringLiteral("ArbitriumRegedit")}) {
        ok = Registry::remove(Registry::Hive::HKCR,
                              QStringLiteral("DesktopBackground\\Shell\\") + verb,
                              QStringLiteral("HasLUAShield"))
             && ok;
    }

    return ok;
}

/// The ownership entries of 0.9.5.
///
/// 0.9.4 briefly put take-ownership on the verb named `runas`, which does elevate but
/// costs more than it gives: a key holds that name only once, so there was nowhere left
/// to put the entry that hands ownership back, and on an .exe the shell picks its own
/// "Run as administrator" over ours. Both entries now call Arbitrium, which elevates by
/// manifest. The keys under the old name are no longer named by any tweak.
bool ownershipVerbs()
{
    bool ok = true;
    for (const QString &path : {QStringLiteral("*\\shell\\runas"),
                                QStringLiteral("Directory\\shell\\runas")}) {
        ok = Registry::removeKey(Registry::Hive::HKCR, path) && ok;
    }
    return ok;
}

struct Step
{
    int level;
    bool (*run)();
};

constexpr Step Steps[] = {
    { 1, &contextMenuRework },
    { 2, &ownershipVerbs },
};

} // namespace

void Migration::runOnce()
{
    QSettings store;
    const int done = store.value(KeyLevel, 0).toInt();

    // A step that could not finish — running without an elevated token is the way that
    // happens — is left unrecorded, so the next launch tries it again rather than
    // pretending the leftovers are gone.
    int reached = done;
    for (const Step &step : Steps) {
        if (step.level <= done)
            continue;
        if (!step.run())
            break;
        reached = step.level;
    }

    if (reached != done)
        store.setValue(KeyLevel, reached);
}
