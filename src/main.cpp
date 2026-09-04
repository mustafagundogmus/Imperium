#include "action.h"
#include "actionengine.h"
#include "catalog.h"
#include "css.h"
#include "mainwindow.h"
#include "migration.h"
#include "ownership.h"
#include "preset.h"
#include "registry.h"
#include "startup.h"
#include "views/settingspage.h"
#include "views/setupwizard.h"
#include "sysinfo.h"
#include "i18n.h"
#include "theme.h"
#include "tweakengine.h"
#include "widgets/buttons.h"
#include "widgets/splashscreen.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QIcon>
#include "widgets/dialog.h"
#include <QMouseEvent>
#include <QTextStream>
#include <QTimer>

namespace {

/// Base palette. Most widgets paint themselves, but Qt's own bits (text cursor,
/// selection, tooltips) need to be told about the dark ground too.
void applyPalette(QApplication &app)
{
    using namespace Theme;

    QPalette pal = app.palette();
    pal.setColor(QPalette::Window, Color::Window());
    pal.setColor(QPalette::WindowText, Color::TextPrimary());
    pal.setColor(QPalette::Base, Color::Surface());
    pal.setColor(QPalette::AlternateBase, Color::SurfaceHover());
    pal.setColor(QPalette::Text, Color::TextPrimary());
    pal.setColor(QPalette::PlaceholderText, Color::Placeholder());
    pal.setColor(QPalette::Button, Color::Surface());
    pal.setColor(QPalette::ButtonText, Color::TextPrimary());
    pal.setColor(QPalette::ToolTipBase, Color::Surface());
    pal.setColor(QPalette::ToolTipText, Color::TextMono());
    pal.setColor(QPalette::Highlight, Theme::accent());
    pal.setColor(QPalette::HighlightedText, Color::OnAccent());
    app.setPalette(pal);

    app.setStyleSheet(QStringLiteral("QToolTip {"
                                     "  background: %1;"
                                     "  color: %2;"
                                     "  border: 1px solid %3;"
                                     "  padding: 3px 6px;"
                                     "}")
                          .arg(Color::Surface().name(), Color::TextMono().name(),
                               Color::BorderControl().name()));
}

/// Delivers a real press/release pair to \a w, the same way the window manager would.
void clickWidget(QWidget *w)
{
    const QPointF local(w->width() / 2.0, w->height() / 2.0);
    const QPointF global = w->mapToGlobal(local);

    QMouseEvent press(QEvent::MouseButtonPress, local, global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &press);

    QMouseEvent release(QEvent::MouseButtonRelease, local, global,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &release);

    QApplication::processEvents();
}

/// `--self-test`: drives the three window buttons through their real signal path and
/// writes the resulting window state to a file. Verifying this with synthetic desktop
/// clicks is unreliable — any always-on-top overlay swallows them before they arrive.
void runSelfTest(MainWindow &window, const QString &path)
{
    const QList<WindowButton *> buttons = window.findChildren<WindowButton *>();

    QStringList lines;
    lines << QStringLiteral("buttons found: %1").arg(buttons.size());

    // Flushed after every block: a self test that dies half way should still say how far
    // it got, and an empty file is a worse bug report than a truncated one.
    const auto flush = [&lines, &path] {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            for (const QString &line : std::as_const(lines))
                out << line << Qt::endl;
        }
    };
    flush();

    // Registry round-trip. Deliberately runs against a scratch key of our own rather
    // than a real tweak, so verifying the write path never changes a user setting.
    {
        using namespace Registry;
        // Not `path` — that is this function's own parameter, the file the report goes
        // to, and shadowing it here made the flush lambda above read like it wrote to
        // the registry key.
        const QString scratch = QStringLiteral("Software\\Arbitrium\\SelfTest");
        const QString name = QStringLiteral("Probe");
        QString error;

        const bool wrote = write(Hive::HKCU, scratch, name, QStringLiteral("DWORD"),
                                 QStringLiteral("4242"), &error);
        const Value readBack = read(Hive::HKCU, scratch, name);
        const bool removed = remove(Hive::HKCU, scratch, name, &error);
        const Value afterRemove = read(Hive::HKCU, scratch, name);

        // The key-shaped switch: an empty default value written into a key that did not
        // exist, undone by deleting the key rather than the value. Both halves are easy
        // to get subtly wrong — a null buffer on the way in, a hollow key left behind on
        // the way out — so the scratch key exercises them for real.
        const QString keyPath = scratch + QStringLiteral("\\KeyShaped");
        const bool madeKey = write(Hive::HKCU, keyPath, QString(), QStringLiteral("SZ"),
                                   QString(), &error);
        const Value defaultValue = read(Hive::HKCU, keyPath, QString());

        // Binary, which the startup entries write: twelve bytes through the catalogue's
        // comma-separated hex and back again.
        const QString blob = Startup::enabledBlob();
        const bool wroteBlob = write(Hive::HKCU, scratch, QStringLiteral("Blob"),
                                     QStringLiteral("BINARY"), blob, &error);
        const Value blobBack = read(Hive::HKCU, scratch, QStringLiteral("Blob"));
        remove(Hive::HKCU, scratch, QStringLiteral("Blob"), &error);
        const bool droppedKey = removeKey(Hive::HKCU, keyPath, &error);

        // MULTI_SZ, which used to be written as a plain REG_SZ: a list read back as a
        // single string is the shape a journalled revert would have restored wrong.
        const QString list = QStringLiteral("bir") + QChar(u'\0') + QStringLiteral("iki");
        const bool wroteList = write(Hive::HKCU, scratch, QStringLiteral("List"),
                                     QStringLiteral("MULTI_SZ"), list, &error);
        const Value listBack = read(Hive::HKCU, scratch, QStringLiteral("List"));
        remove(Hive::HKCU, scratch, QStringLiteral("List"), &error);

        lines << QStringLiteral("registry write   -> %1").arg(wrote)
              << QStringLiteral("registry read    -> exists=%1 type=%2 data=%3")
                     .arg(readBack.exists).arg(readBack.type, readBack.data)
              << QStringLiteral("registry delete  -> %1 (gone=%2)")
                     .arg(removed).arg(!afterRemove.exists)
              << QStringLiteral("default value    -> wrote=%1 exists=%2 type=%3 empty=%4")
                     .arg(madeKey).arg(defaultValue.exists)
                     .arg(defaultValue.type).arg(defaultValue.data.isEmpty())
              << QStringLiteral("binary round     -> wrote=%1 eşit=%2")
                     .arg(wroteBlob).arg(blobBack.data.compare(blob, Qt::CaseInsensitive) == 0)
              << QStringLiteral("key delete       -> %1 (gone=%2)")
                     .arg(droppedKey).arg(!keyExists(Hive::HKCU, keyPath))
              << QStringLiteral("multi_sz round   -> wrote=%1 type=%2 eşit=%3")
                     .arg(wroteList).arg(listBack.type).arg(listBack.data == list)
              << QStringLiteral("elevated         -> %1").arg(TweakEngine::isElevated());
    }

    // Undoing one journalled write, against a key something else lives in.
    //
    // This is the 0.9.9 bug written down as a test. revert() used to delete the whole key
    // whenever the journal said the value had not been there before — and `keyExisted` is
    // one instant's snapshot, while the keys are shared: thirteen catalogue tweaks write
    // into the Windows Update policy key alone. Undoing one of them took the other twelve,
    // and anything a domain policy had put there since, and reported success.
    //
    // Two scenarios, because the fix has two halves that pull against each other: the key
    // must survive while somebody else is in it, and must still go when it is empty.
    {
        using namespace Registry;
        const QString shared = QStringLiteral("Software\\Arbitrium\\SelfTest\\Shared");
        const QString alone = QStringLiteral("Software\\Arbitrium\\SelfTest\\Alone");
        QString error;

        TweakEngine::JournalEntry entry;
        entry.hive = QStringLiteral("HKCU");
        entry.existed = false;      // the value was not there before the write…
        entry.keyExisted = false;   // …and neither was the key
        entry.desired = QStringLiteral("1");

        // One key, two values, as two tweaks writing into the same policy key.
        write(Hive::HKCU, shared, QStringLiteral("Mine"), QStringLiteral("DWORD"),
              QStringLiteral("1"), &error);
        write(Hive::HKCU, shared, QStringLiteral("Neighbour"), QStringLiteral("DWORD"),
              QStringLiteral("2"), &error);

        TweakEngine engine;
        entry.path = shared;
        entry.value = QStringLiteral("Mine");
        const bool undidShared = engine.revert(entry, &error);
        const bool mineGone = !read(Hive::HKCU, shared, QStringLiteral("Mine")).exists;
        const bool neighbourKept = read(Hive::HKCU, shared, QStringLiteral("Neighbour")).exists;
        const bool sharedKeyKept = keyExists(Hive::HKCU, shared);

        // The same undo where nothing else is in the key: this one should take the key.
        write(Hive::HKCU, alone, QStringLiteral("Only"), QStringLiteral("DWORD"),
              QStringLiteral("1"), &error);
        entry.path = alone;
        entry.value = QStringLiteral("Only");
        const bool undidAlone = engine.revert(entry, &error);
        const bool aloneKeyGone = !keyExists(Hive::HKCU, alone);

        // And a line that recorded a whole-key deletion, which one value cannot undo.
        entry.desired = DeleteKeySentinel;
        QString refusal;
        const bool refusedKeyDelete = !engine.revert(entry, &refusal);

        removeKey(Hive::HKCU, QStringLiteral("Software\\Arbitrium\\SelfTest"), &error);

        lines << QStringLiteral("geri al · paylaşılan -> undo=%1 değer gitti=%2 komşu duruyor=%3 anahtar duruyor=%4")
                     .arg(undidShared).arg(mineGone).arg(neighbourKept).arg(sharedKeyKept)
              << QStringLiteral("geri al · tek       -> undo=%1 boş anahtar gitti=%2")
                     .arg(undidAlone).arg(aloneKeyGone)
              << QStringLiteral("geri al · anahtar   -> reddedildi=%1 '%2'")
                     .arg(refusedKeyDelete).arg(refusal);
    }

    // Build-bound tweaks: which ones this machine cannot use, and why.
    {
        int total = 0;
        int locked = 0;
        QStringList named;
        forEachTweak(Catalog::instance(), [&](const Tweak &t) {
            if (t.locked)
                ++locked;
            if (t.applicable)
                return;
            ++total;
            if (named.size() < 3)
                named << QStringLiteral("%1 (%2)").arg(t.name, t.requirement);
        });

        lines << QStringLiteral("build            -> %1").arg(SysInfo::buildNumber())
              << QStringLiteral("uyumsuz tweak    -> %1 · %2").arg(total).arg(named.join(QStringLiteral(" | ")))
              << QStringLiteral("kilitli hizmet   -> %1").arg(locked);
    }

    // The interface face is swappable at runtime, and every style is cached, so this
    // checks the cache actually invalidates — and that the swap is reversible.
    {
        const QString before = Theme::typeface();
        const QString family1 = Theme::Font::tweakName().family();
        const qreal line1 = Css::normalLine(Theme::Font::tweakName());

        // Persist::No on both: the self test must leave the machine exactly as it found
        // it, and these two calls used to write the swap and the restore to QSettings.
        Theme::setTypeface(QStringLiteral("saira"), Theme::Persist::No);
        const QString family2 = Theme::Font::tweakName().family();
        const qreal line2 = Css::normalLine(Theme::Font::tweakName());

        Theme::setTypeface(before, Theme::Persist::No);
        const QString family3 = Theme::Font::tweakName().family();

        lines << QStringLiteral("typeface swap    -> %1 (%2px) -> %3 (%4px) -> %5")
                     .arg(family1).arg(line1, 0, 'f', 1).arg(family2)
                     .arg(line2, 0, 'f', 1).arg(family3)
              << QStringLiteral("mono unchanged   -> %1").arg(Theme::Font::monoMeta().family());
    }

    // A language switch rebuilds SettingsPage's whole widget tree, and the row that just
    // triggered it — the language picker itself — sits inside that same tree. Doing the
    // rebuild with a plain `delete` while its own click handler was still unwinding was
    // exactly this: a widget destroying itself mid-event. Live against the real page
    // MainWindow already built, not a scratch instance, since a scratch page would not
    // have anything wired to the signal this test fires.
    {
        SettingsPage *settings = window.findChild<SettingsPage *>();
        const QString before = Locale::language();
        const int rowsBefore = settings ? settings->rowCount() : -1;

        Locale::setLanguage(QStringLiteral("en"));
        const int rowsAfterEn = settings ? settings->rowCount() : -1;
        const QString labelEn = Locale::tr(QStringLiteral("sidebar.settings"));

        Locale::setLanguage(before);
        const int rowsAfterRestore = settings ? settings->rowCount() : -1;

        lines << QStringLiteral("dil degisimi     -> sayfa=%1 satir %2->%3->%4 · en etiket='%5'")
                     .arg(settings != nullptr).arg(rowsBefore).arg(rowsAfterEn)
                     .arg(rowsAfterRestore).arg(labelEn);
    }

    // Presets carry a position per tweak, not a bool. This writes one, reads it back, and
    // then reads a version-1 file by hand, because old presets have to keep loading.
    {
        const Tweak *service = nullptr;
        const Tweak *choice = nullptr;
        forEachTweak(Catalog::instance(), [&](const Tweak &t) {
            if (!service && t.id.startsWith(QLatin1String("svc-")) && t.editable())
                service = &t;
            if (!choice && t.id == QLatin1String("ch-ipv6"))
                choice = &t;
        });

        const QString file = QDir(Preset::directory()).filePath(QStringLiteral("self-test.xml"));
        QHash<QString, int> written;
        if (service)
            written.insert(service->id, 3);    // Devre dışı
        if (choice)
            written.insert(choice->id, 2);     // Teredo kapalı

        QString error;
        const bool saved = Preset::save(file, QStringLiteral("self-test"), written, &error);
        const Preset::LoadResult back = Preset::load(file);

        bool same = saved && back.ok && back.positions.size() == written.size();
        for (auto it = written.cbegin(); same && it != written.cend(); ++it)
            same = back.positions.value(it.key(), -1) == it.value();

        // A version-1 file, spelled the way the old writer spelled it.
        const QString legacyPath = QDir(Preset::directory()).filePath(QStringLiteral("self-test-v1.xml"));
        QFile legacy(legacyPath);
        int legacyPosition = -1;
        if (legacy.open(QIODevice::WriteOnly | QIODevice::Truncate) && choice) {
            // Single-quoted attributes: XML allows them, and they keep this readable.
            const QString xml =
                QStringLiteral("<?xml version='1.0' encoding='UTF-8'?>\n"
                               "<arbitrium-preset version='1'>\n"
                               "  <meta name='eski'/>\n"
                               "  <tweaks>\n"
                               "    <tweak id='%1' on='true'/>\n"
                               "  </tweaks>\n"
                               "</arbitrium-preset>\n").arg(choice->id);
            legacy.write(xml.toUtf8());
            legacy.close();
            legacyPosition = Preset::load(legacyPath).positions.value(choice->id, -1);
        }

        QFile::remove(file);
        QFile::remove(legacyPath);

        lines << QStringLiteral("ön ayar turu     -> yazıldı=%1 okundu=%2 aynı=%3 (%4 giriş)")
                     .arg(saved).arg(back.ok).arg(same).arg(back.positions.size())
              << QStringLiteral("eski biçim       -> on=true → konum %1").arg(legacyPosition);

        // The .reg exporter, over the same two tweaks: one DWORD-shaped and one that
        // writes several values, so the encoding gets exercised rather than just called.
        const QString regPath = QDir(Preset::directory()).filePath(QStringLiteral("self-test.reg"));
        QStringList ids = written.keys();
        ids.sort();
        const int values = Preset::exportRegFile(regPath, ids, written, &error);

        QString firstKey;
        QFile reg(regPath);
        if (reg.open(QIODevice::ReadOnly)) {
            const QByteArray raw = reg.readAll();
            const QString text = QString::fromUtf16(reinterpret_cast<const char16_t *>(raw.constData() + 2),
                                                    int(raw.size() - 2) / 2);
            for (const QString &line : text.split(QStringLiteral("\r\n")))
                if (line.startsWith(QLatin1Char('['))) {
                    firstKey = line;
                    break;
                }
            reg.close();
        }
        QFile::remove(regPath);

        lines << QStringLiteral("reg dışa aktarma -> %1 değer · ilk anahtar %2")
                     .arg(values).arg(firstKey);
    }
    flush();

    // The action engine, end to end, against a script that touches nothing: the plumbing
    // being verified is the script file, the PowerShell invocation and the captured
    // output — not what any real action would do to the machine.
    {
        Action probe;
        probe.id = QStringLiteral("self-test");
        probe.name = QStringLiteral("Self test");
        probe.run = QStringList{QStringLiteral("Write-Output 'eylem motoru calisiyor'")};

        ActionEngine engine;
        QString captured;
        bool ok = false;
        bool done = false;
        QObject::connect(&engine, &ActionEngine::finished, &engine,
                         [&](const QString &, bool success, const QString &output) {
                             ok = success;
                             captured = output;
                             done = true;
                         });

        engine.run(probe);
        QElapsedTimer clock;
        clock.start();
        while (!done && clock.elapsed() < 20000)
            QApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);

        lines << QStringLiteral("eylem motoru     -> bitti=%1 ok=%2 çıktı='%3'")
                     .arg(done).arg(ok).arg(captured)
              << QStringLiteral("eylem sayısı     -> %1").arg(ActionCatalog::instance().total());
    }
    flush();

    if (buttons.size() == 3) {
        WindowButton *minimize = buttons.at(0);
        WindowButton *maximize = buttons.at(1);
        WindowButton *close = buttons.at(2);

        clickWidget(minimize);
        lines << QStringLiteral("minimize -> isMinimized=%1").arg(window.isMinimized());
        window.showNormal();
        QApplication::processEvents();

        clickWidget(maximize);
        lines << QStringLiteral("maximize -> isMaximized=%1 size=%2x%3")
                     .arg(window.isMaximized())
                     .arg(window.width()).arg(window.height());

        clickWidget(maximize);
        lines << QStringLiteral("restore  -> isMaximized=%1 size=%2x%3")
                     .arg(window.isMaximized())
                     .arg(window.width()).arg(window.height());

        clickWidget(close);
        lines << QStringLiteral("close    -> isVisible=%1").arg(window.isVisible());
    }

    flush();
    QCoreApplication::quit();
}

} // namespace

// CMake defines this from project(... VERSION ...); the fallback is only for a build that
// bypasses it. Never type a version here — the update check compares against this string,
// so a stale one makes the app announce its own release as an upgrade.
#ifndef ARBITRIUM_VERSION
#  define ARBITRIUM_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Arbitrium"));
    QApplication::setApplicationVersion(QStringLiteral(ARBITRIUM_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Arbitrium"));
    QApplication::setOrganizationDomain(QStringLiteral("arbitrium.local"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/tweaker.ico")));

    Theme::initFonts();
    Locale::init();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Windows Tweaker"));
    parser.addHelpOption();
    parser.addVersionOption();

    // The mockup exposes exactly these two design props; they are settings here, not UI.
    QCommandLineOption accentOption({QStringLiteral("a"), QStringLiteral("accent")},
                                    QStringLiteral("Accent colour, e.g. #7FB8A4."),
                                    QStringLiteral("colour"));
    QCommandLineOption compactOption(QStringLiteral("compact"),
                                     QStringLiteral("Denser tweak rows (4px instead of 7px padding)."));
    QCommandLineOption themeOption(QStringLiteral("theme"),
                                   QStringLiteral("Appearance: dark, light, midnight, sepia, ocean, forest, dusk, "
                                                  "rose, mist, contrast, meadow, lilac."),
                                   QStringLiteral("name"));
    QCommandLineOption typefaceOption(QStringLiteral("typeface"),
                                      QStringLiteral("Interface face: plex, monda, opensans, "
                                                     "oxygen, redhat or saira."),
                                      QStringLiteral("id"));
    QCommandLineOption searchOption(QStringLiteral("search"),
                                    QStringLiteral("Open with this text in the search box."),
                                    QStringLiteral("text"));
    QCommandLineOption categoryOption(QStringLiteral("category"),
                                      QStringLiteral("Category to open, e.g. priv."),
                                      QStringLiteral("id"));
    QCommandLineOption shotOption(QStringLiteral("screenshot"),
                                  QStringLiteral("Save a PNG of the window and exit."),
                                  QStringLiteral("path"));
    QCommandLineOption shotDelayOption(QStringLiteral("screenshot-delay"),
                                       QStringLiteral("Milliseconds to wait first (default 900); "
                                                      "raise it to let the live chart fill."),
                                       QStringLiteral("ms"), QStringLiteral("900"));
    QCommandLineOption windowOption(QStringLiteral("window"),
                                    QStringLiteral("Open with the card this size, e.g. 1100x680 - "
                                                   "with --screenshot, to photograph a narrow layout."),
                                    QStringLiteral("WxH"));
    parser.addOption(accentOption);
    parser.addOption(compactOption);
    parser.addOption(themeOption);
    parser.addOption(typefaceOption);
    parser.addOption(categoryOption);
    parser.addOption(searchOption);
    QCommandLineOption selfTestOption(QStringLiteral("self-test"),
                                      QStringLiteral("Exercise the window buttons and write the "
                                                     "result to a file, then exit."),
                                      QStringLiteral("path"));
    // How the shell menu entries reach us. Arbitrium is manifested requireAdministrator,
    // so being started at all means the token is already the one these need.
    QCommandLineOption ownOption(QStringLiteral("own"),
                                 QStringLiteral("Take ownership of a file or folder, then exit."),
                                 QStringLiteral("path"));
    QCommandLineOption disownOption(QStringLiteral("disown"),
                                    QStringLiteral("Give a file or folder back to "
                                                   "TrustedInstaller, then exit."),
                                    QStringLiteral("path"));
    QCommandLineOption shellOption(QStringLiteral("shell"),
                                   QStringLiteral("Window shell for this run: classic or fluent."),
                                   QStringLiteral("name"));
    parser.addOption(shellOption);
    parser.addOption(shotOption);
    parser.addOption(shotDelayOption);
    parser.addOption(windowOption);
    parser.addOption(selfTestOption);
    parser.addOption(ownOption);
    parser.addOption(disownOption);
    parser.process(app);

    if (parser.isSet(ownOption) || parser.isSet(disownOption)) {
        const bool taking = parser.isSet(ownOption);
        const QString target = taking ? parser.value(ownOption) : parser.value(disownOption);
        const Ownership::Result result = taking ? Ownership::take(target)
                                                : Ownership::giveBack(target);

        // No parent: this branch runs before any window exists, so the dialog centres
        // itself on the screen rather than on an application that is not there.
        Dialog::inform(nullptr, Locale::tr(QStringLiteral("own.dialogTitle")),
                       result.summary, Locale::tr(QStringLiteral("apply.close")),
                       result.detail);
        return result.ok ? 0 : 1;
    }

    // All four are overrides for this run only. They used to go through the persisting
    // setters, so `--accent #ff0000` to look at a colour, or `--theme light` to take one
    // screenshot, quietly became the look the app opened with from then on.
    if (parser.isSet(accentOption))
        Theme::setAccent(QColor(parser.value(accentOption)), Theme::Persist::No);
    if (parser.isSet(compactOption))
        Theme::setCompact(true, Theme::Persist::No);
    if (parser.isSet(typefaceOption))
        Theme::setTypeface(parser.value(typefaceOption), Theme::Persist::No);
    if (parser.isSet(shellOption)) {
        const QString name = parser.value(shellOption).toLower();
        if (name == QLatin1String("fluent") || name == QLatin1String("classic"))
            Theme::setShell(Theme::shellFromString(name), Theme::Persist::No);
        else
            qWarning("--shell takes classic or fluent; ignoring '%s'", qUtf8Printable(name));
    }
    if (parser.isSet(themeOption)) {
        // Named, not "light or else dark": the old test made every misspelling mean dark,
        // so `--theme lite` silently did the opposite of what it asked for.
        const QString name = parser.value(themeOption).toLower();
        if (name == QLatin1String("light"))
            Theme::setAppearance(Theme::Appearance::Light, Theme::Persist::No);
        else if (name == QLatin1String("dark"))
            Theme::setAppearance(Theme::Appearance::Dark, Theme::Persist::No);
        else if (name == QLatin1String("midnight"))
            Theme::setAppearance(Theme::Appearance::Midnight, Theme::Persist::No);
        else if (name == QLatin1String("sepia"))
            Theme::setAppearance(Theme::Appearance::Sepia, Theme::Persist::No);
        else if (name == QLatin1String("ocean"))
            Theme::setAppearance(Theme::Appearance::Ocean, Theme::Persist::No);
        else if (name == QLatin1String("forest"))
            Theme::setAppearance(Theme::Appearance::Forest, Theme::Persist::No);
        else if (name == QLatin1String("dusk"))
            Theme::setAppearance(Theme::Appearance::Dusk, Theme::Persist::No);
        else if (name == QLatin1String("rose"))
            Theme::setAppearance(Theme::Appearance::Rose, Theme::Persist::No);
        else if (name == QLatin1String("mist"))
            Theme::setAppearance(Theme::Appearance::Mist, Theme::Persist::No);
        else if (name == QLatin1String("contrast"))
            Theme::setAppearance(Theme::Appearance::Contrast, Theme::Persist::No);
        else if (name == QLatin1String("meadow"))
            Theme::setAppearance(Theme::Appearance::Meadow, Theme::Persist::No);
        else if (name == QLatin1String("lilac"))
            Theme::setAppearance(Theme::Appearance::Lilac, Theme::Persist::No);
        else
            qWarning("--theme takes dark, light, midnight, sepia, ocean, forest, dusk, "
                     "rose, mist, contrast, meadow or lilac; ignoring '%s'",
                     qUtf8Printable(name));
    }

    applyPalette(app);
    // Qt's own palette and the tooltip skin are not repainted by our widgets, so they
    // have to be rebuilt whenever the appearance or the accent changes.
    QObject::connect(Theme::notifier(), &Theme::Notifier::appearanceChanged,
                     &app, [&app] { applyPalette(app); });
    QObject::connect(Theme::notifier(), &Theme::Notifier::accentChanged,
                     &app, [&app] { applyPalette(app); });

    // Anything an older build wrote in a shape this one no longer knows how to undo is
    // cleared here, before the catalogue is read for the first time.
    Migration::runOnce();

    // The splash paints while the window is being built behind it: the catalogue, the
    // services, the machine's facts. --screenshot and --self-test skip it, because both
    // are meant to produce a file without a human watching.
    const bool automated = parser.isSet(shotOption) || parser.isSet(selfTestOption);

    // First launch on this machine: whoever is running it now picks a language and a
    // look before anything else opens. A local event loop blocks main() here exactly
    // the way QDialog::exec() would — everything after this point runs assuming setup
    // is already done, which keeps it from having to know whether it just happened.
    if (!automated && Locale::isFirstRunPending()) {
        SetupWizard wizard;
        QEventLoop setupLoop;
        QObject::connect(&wizard, &SetupWizard::finished, &setupLoop, &QEventLoop::quit);
        wizard.show();
        setupLoop.exec();
    }
    SplashScreen *splash = nullptr;
    if (!automated) {
        splash = new SplashScreen;
        splash->show();
        QApplication::processEvents();
    }

    MainWindow window;
    if (parser.isSet(categoryOption))
        window.showCategory(parser.value(categoryOption));
    if (parser.isSet(searchOption))
        window.showSearch(parser.value(searchOption));
    if (splash) {
        splash->finish(&window);
        splash->deleteLater();
    } else {
        window.show();
    }

    if (parser.isSet(windowOption)) {
        const QStringList parts = parser.value(windowOption).split(QLatin1Char('x'));
        const QSize card(parts.value(0).toInt(), parts.value(1).toInt());
        if (parts.size() == 2 && !card.isEmpty())
            window.resizeCard(card);
        else
            qWarning("--window takes WIDTHxHEIGHT, e.g. 1100x680; ignoring '%s'",
                     qUtf8Printable(parser.value(windowOption)));
    }

    if (parser.isSet(selfTestOption)) {
        const QString path = parser.value(selfTestOption);
        QTimer::singleShot(800, &app, [&window, path] { runSelfTest(window, path); });
    }

    if (parser.isSet(shotOption)) {
        const QString path = parser.value(shotOption);
        const int delay = qMax(0, parser.value(shotDelayOption).toInt());
        QTimer::singleShot(delay, &app, [&window, path] {
            window.grabCard().save(path);
            QCoreApplication::quit();
        });
    }

    return QApplication::exec();
}
