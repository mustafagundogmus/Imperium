#include "mainwindow.h"
#include "registry.h"
#include "theme.h"
#include "tweakengine.h"
#include "widgets/buttons.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
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

    // Registry round-trip. Deliberately runs against a scratch key of our own rather
    // than a real tweak, so verifying the write path never changes a user setting.
    {
        using namespace Registry;
        const QString path = QStringLiteral("Software\\Arbitrium\\SelfTest");
        const QString name = QStringLiteral("Probe");
        QString error;

        const bool wrote = write(Hive::HKCU, path, name, QStringLiteral("DWORD"),
                                 QStringLiteral("4242"), &error);
        const Value readBack = read(Hive::HKCU, path, name);
        const bool removed = remove(Hive::HKCU, path, name, &error);
        const Value afterRemove = read(Hive::HKCU, path, name);

        lines << QStringLiteral("registry write   -> %1").arg(wrote)
              << QStringLiteral("registry read    -> exists=%1 type=%2 data=%3")
                     .arg(readBack.exists).arg(readBack.type, readBack.data)
              << QStringLiteral("registry delete  -> %1 (gone=%2)")
                     .arg(removed).arg(!afterRemove.exists)
              << QStringLiteral("elevated         -> %1").arg(TweakEngine::isElevated());
    }

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

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (const QString &line : std::as_const(lines))
            out << line << '\n';
    }
    QCoreApplication::quit();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Arbitrium"));
    QApplication::setApplicationVersion(QStringLiteral("0.9.2"));
    QApplication::setOrganizationName(QStringLiteral("Arbitrium"));
    QApplication::setOrganizationDomain(QStringLiteral("arbitrium.local"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/tweaker.ico")));

    Theme::initFonts();

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
                                   QStringLiteral("Appearance: dark or light."),
                                   QStringLiteral("name"));
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
    parser.addOption(accentOption);
    parser.addOption(compactOption);
    parser.addOption(themeOption);
    parser.addOption(categoryOption);
    QCommandLineOption selfTestOption(QStringLiteral("self-test"),
                                      QStringLiteral("Exercise the window buttons and write the "
                                                     "result to a file, then exit."),
                                      QStringLiteral("path"));
    parser.addOption(shotOption);
    parser.addOption(shotDelayOption);
    parser.addOption(selfTestOption);
    parser.process(app);

    if (parser.isSet(accentOption))
        Theme::setAccent(QColor(parser.value(accentOption)));
    if (parser.isSet(compactOption))
        Theme::setCompact(true);
    if (parser.isSet(themeOption))
        Theme::setAppearance(parser.value(themeOption).compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0
                                 ? Theme::Appearance::Light
                                 : Theme::Appearance::Dark);

    applyPalette(app);
    // Qt's own palette and the tooltip skin are not repainted by our widgets, so they
    // have to be rebuilt whenever the appearance or the accent changes.
    QObject::connect(Theme::notifier(), &Theme::Notifier::appearanceChanged,
                     &app, [&app] { applyPalette(app); });
    QObject::connect(Theme::notifier(), &Theme::Notifier::accentChanged,
                     &app, [&app] { applyPalette(app); });

    MainWindow window;
    if (parser.isSet(categoryOption))
        window.showCategory(parser.value(categoryOption));
    window.show();

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
