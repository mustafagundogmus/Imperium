#include "godmodepage.h"

#include "../i18n.h"
#include "../theme.h"
#include "../widgets/buttons.h"
#include "../widgets/searchfield.h"
#include "../widgets/sectionheader.h"
#include "../widgets/settingrow.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

/// What a target string is, decided from the string itself rather than from a field in the
/// JSON. A "kind" column would be a second place to keep the same fact, and the shapes are
/// already unambiguous: nothing in settings-links.json is an absolute path, so a colon can
/// only mean a scheme.
enum class Kind {
    Uri,       ///< ms-settings:… — Windows' own Settings app
    Shell,     ///< shell:… — a shell folder, including the all-tasks one
    Console,   ///< a .msc console document
    Applet,    ///< a .cpl control-panel applet
    Program,   ///< a .exe under System32 or %SystemRoot%
};

Kind kindOf(const QString &target)
{
    // shell: first, because it is also a scheme and would otherwise be taken for a URI.
    if (target.startsWith(QLatin1String("shell:"), Qt::CaseInsensitive))
        return Kind::Shell;
    if (target.contains(QLatin1Char(':')))
        return Kind::Uri;
    if (target.endsWith(QLatin1String(".msc"), Qt::CaseInsensitive))
        return Kind::Console;
    if (target.endsWith(QLatin1String(".cpl"), Qt::CaseInsensitive))
        return Kind::Applet;
    return Kind::Program;
}

/// %SystemRoot%\System32, asked of the API rather than read out of the environment.
///
/// tilauncherpage.cpp reads $SystemRoot for the same job, and for a path the user is about
/// to see in a text field that is fine. This one is different: the result is fed straight
/// to CreateProcess in an elevated token, and an environment block is inherited — whoever
/// started this process chose what SystemRoot says. GetSystemDirectoryW cannot be lied to
/// that way. The literal fallback is for the case where even that fails, which in practice
/// means a machine already too broken to launch anything.
QString systemDirectory()
{
#ifdef Q_OS_WIN
    wchar_t buffer[MAX_PATH] = {};
    const UINT written = GetSystemDirectoryW(buffer, MAX_PATH);
    if (written > 0 && written < MAX_PATH)
        return QString::fromWCharArray(buffer, int(written));
#endif
    return QStringLiteral("C:\\Windows\\System32");
}

/// …and %SystemRoot% itself, which is not the same directory and is where two of these
/// live. regedit.exe in particular is in C:\Windows and has never been in System32 — what
/// System32 has is regedt32.exe, a stub that launches the real one.
QString windowsDirectory()
{
#ifdef Q_OS_WIN
    wchar_t buffer[MAX_PATH] = {};
    const UINT written = GetWindowsDirectoryW(buffer, MAX_PATH);
    if (written > 0 && written < MAX_PATH)
        return QString::fromWCharArray(buffer, int(written));
#endif
    return QStringLiteral("C:\\Windows");
}

/// One system file's absolute path, or an empty string when it is not on this machine.
///
/// System32 first, then the Windows directory; never the bare name. An empty answer is a
/// real answer and the caller must keep it: gpedit.msc ships only with Pro and above, so on
/// a Home edition this is the function that finds out, rather than a failed launch later.
QString resolveSystemFile(const QString &fileName)
{
    const QString inSystem = systemDirectory() + QLatin1Char('\\') + fileName;
    if (QFileInfo::exists(inSystem))
        return inSystem;
    const QString inWindows = windowsDirectory() + QLatin1Char('\\') + fileName;
    if (QFileInfo::exists(inWindows))
        return inWindows;
    return QString();
}

struct Command
{
    QString program;
    QStringList arguments;
};

/// The program and arguments one non-URI target is launched as, with every path already
/// absolute. An empty program means the target is not available on this machine.
Command commandFor(const QString &target)
{
    switch (kindOf(target)) {
    case Kind::Shell:
        // A shell: path is Explorer's own address syntax, not a registered protocol, so it
        // is an argument to explorer.exe rather than something to open as a URL. Handing it
        // to QDesktopServices instead would percent-encode the braces of the all-tasks GUID
        // and Explorer would be given an address that does not exist.
        return {resolveSystemFile(QStringLiteral("explorer.exe")), {target}};
    case Kind::Console:
        // A .msc is a saved console document, not a program: mmc.exe is what opens one, and
        // QProcess does not go through ShellExecute, so the file association is no help.
        return {resolveSystemFile(QStringLiteral("mmc.exe")), {resolveSystemFile(target)}};
    case Kind::Applet:
        // A .cpl is a DLL with a Control-panel entry point. control.exe is the host Windows
        // itself uses for them.
        return {resolveSystemFile(QStringLiteral("control.exe")), {resolveSystemFile(target)}};
    case Kind::Program:
        return {resolveSystemFile(target), {}};
    case Kind::Uri:
        break;
    }
    return {};
}

QLabel *makeNote(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setWordWrap(true);
    label->setFont(Theme::Font::pageSub());
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, Theme::Color::TextDesc());
    label->setPalette(pal);
    return label;
}

} // namespace

GodModePage::GodModePage(QWidget *parent)
    : QWidget(parent)
{
    build();

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this,
            &GodModePage::retranslate);
}

void GodModePage::build()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                              Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    outer->setSpacing(Theme::Metric::SectionGap);

    // --- what this page is, and the search over it --------------------------
    {
        auto *block = new QWidget(this);
        auto *layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        m_intro = makeNote(block);
        m_intro->setText(Locale::tr(QStringLiteral("godmode.intro")));
        layout->addWidget(m_intro);

        m_search = new SearchField(block);
        m_search->setPlaceholderKey(QStringLiteral("godmode.search.placeholder"));
        // No ⌃K here: that shortcut focuses the sidebar's search, which searches tweaks.
        m_search->setShortcutBadgeVisible(false);
        connect(m_search, &SearchField::textChanged, this, [this] { applyFilter(); });
        layout->addWidget(m_search);

        outer->addWidget(block);
    }

    // --- one block per group ------------------------------------------------
    for (const SettingsLinks::Group &group : SettingsLinks::groups()) {
        Group built;
        built.id = group.id;

        built.block = new QWidget(this);
        auto *layout = new QVBoxLayout(built.block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        built.header = new SectionHeader(
            Locale::tr(QStringLiteral("godmode.group.") + group.id), built.block);
        layout->addWidget(built.header);

        auto *list = new QWidget(built.block);
        auto *listLayout = new QVBoxLayout(list);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(1);

        for (const SettingsLinks::Link &link : group.links) {
            Row row;
            row.link = link;

            if (kindOf(link.target) != Kind::Uri) {
                const Command command = commandFor(link.target);
                row.program = command.program;
                row.arguments = command.arguments;
                // Both halves have to have resolved: an .msc whose mmc.exe was found but
                // whose own document was not is just as unlaunchable as the other way round.
                row.available = !row.program.isEmpty()
                                && !row.arguments.contains(QString());
            }

            row.button = new PillButton(PillButton::Ghost,
                                        Locale::tr(QStringLiteral("godmode.open")), list);
            row.button->setEnabledLook(row.available);

            row.row = new SettingRow(label(link),
                                     row.available
                                         ? link.target
                                         : Locale::tr(QStringLiteral("godmode.unavailable"))
                                               .arg(link.target),
                                     row.button, SettingRow::Trailing, list);

            const int index = int(m_links.size());
            connect(row.button, &PillButton::clicked, this, [this, index] { launch(index); });

            listLayout->addWidget(row.row);
            built.rows.append(index);
            m_links.append(row);
        }

        built.header->setCount(
            Locale::tr(QStringLiteral("godmode.count")).arg(built.rows.size()));

        layout->addWidget(list);
        outer->addWidget(built.block);
        m_groups.append(built);
    }

    // Shown only while a query matches nothing, so the page does not go blank without
    // saying why. It sits after the groups because that is where the last row was.
    m_empty = makeNote(this);
    m_empty->setText(Locale::tr(QStringLiteral("godmode.noMatch")));
    m_empty->hide();
    outer->addWidget(m_empty);

    outer->addStretch(1);
}

QString GodModePage::label(const SettingsLinks::Link &link) const
{
    return Locale::tr(QStringLiteral("godmode.") + link.id);
}

void GodModePage::launch(int index)
{
    if (index < 0 || index >= int(m_links.size()))
        return;
    const Row &row = m_links.at(index);

    if (kindOf(row.link.target) == Kind::Uri) {
        // The same call settingspage.cpp:315 already opens ms-settings:about with. Windows
        // resolves the scheme to the Settings app; nothing of ours runs.
        if (!QDesktopServices::openUrl(QUrl(row.link.target))) {
            Q_EMIT notice(Locale::tr(QStringLiteral("godmode.err.failed"))
                              .arg(label(row.link), row.link.target));
            return;
        }
        Q_EMIT notice(Locale::tr(QStringLiteral("godmode.notice.opened")).arg(label(row.link)));
        return;
    }

    // A backstop, not the path anybody takes. An unavailable row's button was built with
    // setEnabledLook(false) and PillButton only emits clicked() while it is live
    // (buttons.cpp:116), so what actually answers the user is the dimmed button and the
    // row's own "not on this edition" description. It is still worth the four lines: the
    // day somebody makes those buttons live, the alternative is startDetached("") coming
    // back false and an error message naming an empty path.
    if (!row.available) {
        Q_EMIT notice(Locale::tr(QStringLiteral("godmode.err.missing")).arg(row.link.target));
        return;
    }

    if (!QProcess::startDetached(row.program, row.arguments)) {
        Q_EMIT notice(Locale::tr(QStringLiteral("godmode.err.failed"))
                          .arg(label(row.link), QDir::toNativeSeparators(row.program)));
        return;
    }
    Q_EMIT notice(Locale::tr(QStringLiteral("godmode.notice.opened")).arg(label(row.link)));
}

void GodModePage::applyFilter()
{
    const QString needle = m_search ? m_search->text().trimmed() : QString();

    // The target as well as the label, the way MainWindow::matches() searches a tweak's
    // Turkish original alongside its translation: somebody who came here knowing "ncpa.cpl"
    // is looking for the same row as somebody typing "ağ bağlantıları", and only one of the
    // two is on screen.
    const auto hits = [this, &needle](const Row &row) {
        return needle.isEmpty()
               || label(row.link).contains(needle, Qt::CaseInsensitive)
               || row.link.target.contains(needle, Qt::CaseInsensitive);
    };

    int visible = 0;
    for (const Row &row : std::as_const(m_links)) {
        const bool keep = hits(row);
        row.row->setVisible(keep);
        if (keep)
            ++visible;
    }

    for (const Group &group : std::as_const(m_groups)) {
        int here = 0;
        for (int index : group.rows)
            if (hits(m_links.at(index)))
                ++here;
        // Asked of the predicate again rather than read back off the rows: this page lives
        // in a QStackedWidget, so while another page is on screen every row here answers
        // isVisible() with false whatever it was just set to, and every group would collapse.
        //
        // A heading over nothing reads as a group that has been emptied rather than as one
        // the query did not reach, so the whole block goes.
        group.block->setVisible(here > 0);
        group.header->setCount(Locale::tr(QStringLiteral("godmode.count")).arg(here));
    }

    m_empty->setVisible(visible == 0);
}

void GodModePage::retranslate()
{
    m_intro->setText(Locale::tr(QStringLiteral("godmode.intro")));
    m_empty->setText(Locale::tr(QStringLiteral("godmode.noMatch")));

    for (const Group &group : std::as_const(m_groups))
        group.header->setTitle(Locale::tr(QStringLiteral("godmode.group.") + group.id));

    for (const Row &row : std::as_const(m_links)) {
        row.row->setName(label(row.link));
        row.row->setDesc(row.available
                             ? row.link.target
                             : Locale::tr(QStringLiteral("godmode.unavailable"))
                                   .arg(row.link.target));
        row.button->setText(Locale::tr(QStringLiteral("godmode.open")));
    }

    // The labels just changed under it, so the query has to be measured against the new
    // ones — otherwise a filtered page keeps whichever rows the *previous* language matched.
    applyFilter();
}
