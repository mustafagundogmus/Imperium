#include "migration.h"

#include "registry.h"

#include <QSettings>

namespace {

const QString KeyLevel = QStringLiteral("app/migration");

/// The context-menu rework of 0.9.3.
///
/// Take ownership moved to the verb named `runas`, because that is the one name the shell
/// elevates before running — under the old name the command ran with a plain token and
/// could not touch a file TrustedInstaller owned. The old verb keys were left sitting in
/// the shell, showing a second entry that did not work.
///
/// 0.9.5 then moved it back: the app is manifested requireAdministrator, so the verb no
/// longer has to be named `runas` to elevate, and `ArbitriumTakeOwnership` is the name the
/// current catalogue writes again. Which makes this step's key list the *live* one — it
/// spent three releases deleting the entry the user had just applied, on any machine
/// whose HKCU had not yet recorded the level. So it now reads the command first and
/// leaves anything that names this app's own switches alone, exactly as ownershipVerbs()
/// below does. The verb this app writes invokes the executable with --own or --disown;
/// the 0.9.3-era one ran takeown/icacls directly, and nothing else answers to either
/// description.
///
/// The power plan verb kept its key but became a cascading menu, and a verb cannot both
/// carry a command and open a submenu; the command left over from the old shape has to
/// go for the submenu to appear.
bool contextMenuRework()
{
    bool ok = true;

    for (const QString &path : {QStringLiteral("*\\shell\\ArbitriumTakeOwnership"),
                                QStringLiteral("Directory\\shell\\ArbitriumTakeOwnership")}) {
        const Registry::Value command =
            Registry::read(Registry::Hive::HKCR, path + QStringLiteral("\\command"), QString());
        // Containment, not equality: the catalogue expands %ARBITRIUM% to wherever the
        // executable happens to live, so comparing against one spelling would delete a
        // working entry written by a copy at a different path.
        if (command.exists && (command.data.contains(QLatin1String("--own"))
                               || command.data.contains(QLatin1String("--disown"))))
            continue;
        ok = Registry::removeKey(Registry::Hive::HKCR, path) && ok;
    }

    ok = Registry::removeKey(Registry::Hive::HKCR,
                             QStringLiteral("DesktopBackground\\Shell\\ArbitriumPowerPlan\\command"))
         && ok;

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
///
/// `runas` is a name Windows itself defines, though, and it is one any other program is
/// free to register — a plain "Run as administrator on every file type" entry is one of
/// the most widely copied registry tweaks there is. Deleting the key because 0.9.4 might
/// have written it would take that entry with it on every machine that never ran 0.9.4
/// at all. So the command is read first, and the key goes only when it is ours: the verb
/// this app wrote invokes this executable with --own or --disown, and nothing else does.
bool ownershipVerbs()
{
    bool ok = true;
    for (const QString &path : {QStringLiteral("*\\shell\\runas"),
                                QStringLiteral("Directory\\shell\\runas")}) {
        const Registry::Value command =
            Registry::read(Registry::Hive::HKCR, path + QStringLiteral("\\command"), QString());
        if (!command.exists)
            continue;   // nothing there, or somebody else's verb with no command
        if (!command.data.contains(QLatin1String("--own"))
            && !command.data.contains(QLatin1String("--disown")))
            continue;   // somebody else's "Run as administrator" — not ours to delete
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
