#include "tilauncherpage.h"

#include "../i18n.h"
#include "../theme.h"
#include "../trustedinstaller.h"
#include "../widgets/buttons.h"
#include "../widgets/sectionheader.h"
#include "../widgets/settingrow.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>

#include <iterator>

namespace {

/// The tools worth reaching for at this level, resolved to absolute paths so the launcher
/// finds them without a PATH search. A console is mmc with the console as its argument;
/// a row whose `needs` file is missing — gpedit.msc on a Home edition — is not shown,
/// rather than shown and failing.
struct QuickDef
{
    const char *id;
    const char *relative;    ///< the program, under %SystemRoot%
    const char *arguments;
    const char *needs;    ///< a file under %SystemRoot% the row needs, or null
    const char *nameKey;
    const char *descKey;
};

constexpr QuickDef Quicks[] = {
    {"cmd",        "System32\\cmd.exe",                                 "",             nullptr,
     "ti.quick.cmd.name",        "ti.quick.cmd.desc"},
    {"powershell", "System32\\WindowsPowerShell\\v1.0\\powershell.exe", "",             nullptr,
     "ti.quick.powershell.name", "ti.quick.powershell.desc"},
    {"regedit",    "regedit.exe",                                       "",             nullptr,
     "ti.quick.regedit.name",    "ti.quick.regedit.desc"},
    {"explorer",   "explorer.exe",                                      "",             nullptr,
     "ti.quick.explorer.name",   "ti.quick.explorer.desc"},
    {"taskmgr",    "System32\\Taskmgr.exe",                             "",             nullptr,
     "ti.quick.taskmgr.name",    "ti.quick.taskmgr.desc"},
    {"services",   "System32\\mmc.exe",                                 "services.msc", "System32\\services.msc",
     "ti.quick.services.name",   "ti.quick.services.desc"},
    {"devmgmt",    "System32\\mmc.exe",                                 "devmgmt.msc",  "System32\\devmgmt.msc",
     "ti.quick.devmgmt.name",    "ti.quick.devmgmt.desc"},
    {"taskschd",   "System32\\mmc.exe",                                 "taskschd.msc", "System32\\taskschd.msc",
     "ti.quick.taskschd.name",   "ti.quick.taskschd.desc"},
    {"eventvwr",   "System32\\mmc.exe",                                 "eventvwr.msc", "System32\\eventvwr.msc",
     "ti.quick.eventvwr.name",   "ti.quick.eventvwr.desc"},
    {"diskmgmt",   "System32\\mmc.exe",                                 "diskmgmt.msc", "System32\\diskmgmt.msc",
     "ti.quick.diskmgmt.name",   "ti.quick.diskmgmt.desc"},
    {"compmgmt",   "System32\\mmc.exe",                                 "compmgmt.msc", "System32\\compmgmt.msc",
     "ti.quick.compmgmt.name",   "ti.quick.compmgmt.desc"},
    {"gpedit",     "System32\\mmc.exe",                                 "gpedit.msc",   "System32\\gpedit.msc",
     "ti.quick.gpedit.name",     "ti.quick.gpedit.desc"},
};

constexpr int RecentLimit = 8;
const QLatin1String KeyRecentPrograms("ti/recentPrograms");
const QLatin1String KeyRecentArguments("ti/recentArguments");

QString systemRoot()
{
    const QString root = qEnvironmentVariable("SystemRoot");
    return root.isEmpty() ? QStringLiteral("C:\\Windows") : root;
}

QLabel *makeCaption(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setFont(Theme::Font::tweakDesc());
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, Theme::Color::TextDesc());
    label->setPalette(pal);
    return label;
}

/// The one local file a drag carries, or empty when it carries something else.
QString droppedFile(const QMimeData *mime)
{
    if (!mime || !mime->hasUrls())
        return QString();
    const QList<QUrl> urls = mime->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile())
        return QString();
    return QDir::toNativeSeparators(urls.first().toLocalFile());
}

} // namespace

TiLauncherPage::TiLauncherPage(QWidget *parent)
    : QWidget(parent)
{
    // A drop anywhere on the page fills the target field. The two fields would otherwise
    // take the drop themselves and paste the URL as text.
    setAcceptDrops(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                              Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    outer->setSpacing(Theme::Metric::SectionGap);

    // --- what this is ------------------------------------------------------
    {
        auto *block = new QWidget(this);
        auto *layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        m_introHeader = new SectionHeader(Locale::tr(QStringLiteral("ti.title")), block);
        layout->addWidget(m_introHeader);

        m_intro = new QLabel(block);
        m_intro->setWordWrap(true);
        m_intro->setFont(Theme::Font::pageSub());
        QPalette pal = m_intro->palette();
        pal.setColor(QPalette::WindowText, Theme::Color::TextDesc());
        m_intro->setPalette(pal);
        layout->addWidget(m_intro);

        outer->addWidget(block);
    }

    // --- a target of your own ----------------------------------------------
    {
        auto *block = new QWidget(this);
        auto *layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        m_targetHeader = new SectionHeader(Locale::tr(QStringLiteral("ti.target.header")), block);
        layout->addWidget(m_targetHeader);

        m_pathLabel = makeCaption(Locale::tr(QStringLiteral("ti.target.path")), block);
        layout->addWidget(m_pathLabel);

        auto *pathRow = new QHBoxLayout;
        pathRow->setContentsMargins(0, 0, 0, 0);
        pathRow->setSpacing(8);
        m_path = new QLineEdit(block);
        m_path->setPlaceholderText(Locale::tr(QStringLiteral("ti.target.placeholder")));
        m_path->setFont(Theme::Font::tweakName());
        m_path->setFixedHeight(Theme::Metric::SearchHeight);
        m_path->setAcceptDrops(false);
        pathRow->addWidget(m_path, 1);
        m_browse = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("ti.target.browse")), block);
        connect(m_browse, &PillButton::clicked, this, &TiLauncherPage::browse);
        pathRow->addWidget(m_browse, 0);
        layout->addLayout(pathRow);

        m_hint = makeCaption(Locale::tr(QStringLiteral("ti.target.hint")), block);
        m_hint->setWordWrap(true);
        layout->addWidget(m_hint);

        m_argsLabel = makeCaption(Locale::tr(QStringLiteral("ti.target.args")), block);
        layout->addSpacing(4);
        layout->addWidget(m_argsLabel);
        m_args = new QLineEdit(block);
        m_args->setPlaceholderText(Locale::tr(QStringLiteral("ti.target.argsPlaceholder")));
        m_args->setFont(Theme::Font::tweakName());
        m_args->setFixedHeight(Theme::Metric::SearchHeight);
        m_args->setAcceptDrops(false);
        layout->addWidget(m_args);

        auto *launchRow = new QHBoxLayout;
        launchRow->setContentsMargins(0, 0, 0, 0);
        launchRow->addStretch(1);
        m_launch = new PillButton(PillButton::Accent, Locale::tr(QStringLiteral("ti.target.launch")), block);
        connect(m_launch, &PillButton::clicked, this, &TiLauncherPage::launchFromField);
        connect(m_path, &QLineEdit::returnPressed, this, &TiLauncherPage::launchFromField);
        connect(m_args, &QLineEdit::returnPressed, this, &TiLauncherPage::launchFromField);
        launchRow->addWidget(m_launch, 0);
        layout->addSpacing(2);
        layout->addLayout(launchRow);

        outer->addWidget(block);
    }

    // --- what you launched before ------------------------------------------
    {
        m_recentBlock = new QWidget(this);
        auto *layout = new QVBoxLayout(m_recentBlock);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_recentHeader = new SectionHeader(Locale::tr(QStringLiteral("ti.recent.header")), m_recentBlock);
        layout->addWidget(m_recentHeader);

        m_recentList = new QWidget(m_recentBlock);
        m_recentLayout = new QVBoxLayout(m_recentList);
        m_recentLayout->setContentsMargins(0, 0, 0, 0);
        m_recentLayout->setSpacing(1);
        layout->addWidget(m_recentList);

        auto *clearRow = new QHBoxLayout;
        clearRow->setContentsMargins(0, 6, 0, 0);
        clearRow->addStretch(1);
        m_recentClear = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("ti.recent.clear")), m_recentBlock);
        connect(m_recentClear, &PillButton::clicked, this, [this] {
            m_recent.clear();
            saveRecent(m_recent);
            rebuildRecent();
        });
        clearRow->addWidget(m_recentClear, 0);
        layout->addLayout(clearRow);

        outer->addWidget(m_recentBlock);
        m_recent = loadRecent();
    }

    // --- quick launches ----------------------------------------------------
    {
        auto *block = new QWidget(this);
        auto *layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_quickHeader = new SectionHeader(Locale::tr(QStringLiteral("ti.quick.header")), block);
        layout->addWidget(m_quickHeader);

        auto *list = new QWidget(block);
        auto *listLayout = new QVBoxLayout(list);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(1);

        const QString root = systemRoot();
        for (const QuickDef &def : Quicks) {
            if (def.needs
                && !QFileInfo::exists(root + QLatin1Char('\\') + QString::fromLatin1(def.needs)))
                continue;

            Quick quick;
            quick.id = QString::fromLatin1(def.id);
            quick.program = root + QLatin1Char('\\') + QString::fromLatin1(def.relative);
            quick.arguments = QString::fromLatin1(def.arguments);

            quick.button = new PillButton(PillButton::Ghost,
                                          Locale::tr(QStringLiteral("ti.launch")), list);
            SettingRow *row = new SettingRow(Locale::tr(QString::fromLatin1(def.nameKey)),
                                             Locale::tr(QString::fromLatin1(def.descKey)),
                                             quick.button, SettingRow::Trailing, list);
            quick.row = row;

            const QString program = quick.program;
            const QString arguments = quick.arguments;
            connect(quick.button, &PillButton::clicked, this,
                    [this, program, arguments, row] { launch(program, arguments, row); });

            listLayout->addWidget(row);
            m_quick.append(quick);
        }
        m_quickHeader->setCount(Locale::tr(QStringLiteral("actions.count")).arg(m_quick.size()));

        layout->addWidget(list);
        outer->addWidget(block);
    }

    outer->addStretch(1);

    applyInputStyle();
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this,
            &TiLauncherPage::applyInputStyle);
    connect(Theme::notifier(), &Theme::Notifier::accentChanged, this,
            &TiLauncherPage::applyInputStyle);
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this,
            &TiLauncherPage::retranslate);

    retranslate();
    refreshAvailability();
}

void TiLauncherPage::applyInputStyle()
{
    // A QLineEdit is the one place this page steps outside the hand-painted widgets, so it
    // is styled to match them: the surface behind a control, a 1px border, the accent on
    // focus. Rebuilt on a theme change because a stylesheet does not follow the palette.
    const QString sheet = QStringLiteral(
        "QLineEdit {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: %4px;"
        "  padding: 0 8px;"
        "  selection-background-color: %5;"
        "}"
        "QLineEdit:focus { border: 1px solid %5; }")
        .arg(Theme::Color::Surface().name(),
             Theme::Color::TextPrimary().name(),
             Theme::Color::BorderControl().name())
        .arg(Theme::Metric::ControlRadius)
        .arg(Theme::accent().name());
    if (m_path)
        m_path->setStyleSheet(sheet);
    if (m_args)
        m_args->setStyleSheet(sheet);
}

void TiLauncherPage::refreshAvailability()
{
    m_available = TrustedInstaller::available();

    m_intro->setText(m_available ? Locale::tr(QStringLiteral("ti.intro"))
                                  : Locale::tr(QStringLiteral("ti.intro.unavailable")));

    m_path->setEnabled(m_available);
    m_args->setEnabled(m_available);
    m_browse->setEnabledLook(m_available);
    m_launch->setEnabledLook(m_available);
    m_recentClear->setEnabledLook(m_available);
    for (const Quick &quick : std::as_const(m_quick))
        quick.button->setEnabledLook(m_available);
    rebuildRecent();
}

void TiLauncherPage::browse()
{
    if (!m_available)
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, Locale::tr(QStringLiteral("ti.target.browseTitle")), QString(),
        Locale::tr(QStringLiteral("ti.target.filter")));
    if (!path.isEmpty())
        m_path->setText(QDir::toNativeSeparators(path));
}

void TiLauncherPage::dragEnterEvent(QDragEnterEvent *e)
{
    if (m_available && !droppedFile(e->mimeData()).isEmpty())
        e->acceptProposedAction();
}

void TiLauncherPage::dragMoveEvent(QDragMoveEvent *e)
{
    if (m_available && !droppedFile(e->mimeData()).isEmpty())
        e->acceptProposedAction();
}

void TiLauncherPage::dropEvent(QDropEvent *e)
{
    const QString file = droppedFile(e->mimeData());
    if (!m_available || file.isEmpty())
        return;
    m_path->setText(file);
    m_path->setFocus();
    e->acceptProposedAction();
}

void TiLauncherPage::launchFromField()
{
    if (!m_available) {
        Q_EMIT notice(Locale::tr(QStringLiteral("ti.err.notElevated")));
        return;
    }
    const QString target = m_path->text().trimmed();
    if (target.isEmpty()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("ti.err.noTarget")));
        return;
    }
    const QString arguments = m_args->text().trimmed();
    QString resolved;
    if (launch(target, arguments, nullptr, &resolved))
        remember(resolved, arguments);
}

bool TiLauncherPage::launch(const QString &program, const QString &arguments, SettingRow *row,
                            QString *resolved)
{
    if (!m_available) {
        Q_EMIT notice(Locale::tr(QStringLiteral("ti.err.notElevated")));
        return false;
    }

    const TrustedInstaller::Result result =
        TrustedInstaller::launch(program, arguments, QString());
    const QString started = result.ok ? result.program : program;
    if (resolved)
        *resolved = started;

    const QString message = result.ok
                                ? Locale::tr(QStringLiteral("ti.result.launched"))
                                      .arg(QFileInfo(started).fileName())
                                      .arg(result.pid)
                                : (result.detail.isEmpty()
                                       ? result.summary
                                       : QStringLiteral("%1 · %2").arg(result.summary, result.detail));

    if (row)
        row->setDesc(message);
    Q_EMIT notice(result.ok ? Locale::tr(QStringLiteral("ti.result.notice"))
                                  .arg(QFileInfo(started).fileName())
                            : message);
    return result.ok;
}

QVector<TiLauncherPage::Recent> TiLauncherPage::loadRecent()
{
    QSettings store;
    const QStringList programs = store.value(KeyRecentPrograms).toStringList();
    const QStringList arguments = store.value(KeyRecentArguments).toStringList();
    QVector<Recent> recent;
    for (int i = 0; i < programs.size() && i < RecentLimit; ++i)
        recent.append({programs.at(i), arguments.value(i)});
    return recent;
}

void TiLauncherPage::saveRecent(const QVector<Recent> &recent)
{
    QStringList programs;
    QStringList arguments;
    for (const Recent &r : recent) {
        programs.append(r.program);
        arguments.append(r.arguments);
    }
    QSettings store;
    if (programs.isEmpty()) {
        store.remove(KeyRecentPrograms);
        store.remove(KeyRecentArguments);
    } else {
        store.setValue(KeyRecentPrograms, programs);
        store.setValue(KeyRecentArguments, arguments);
    }
}

void TiLauncherPage::remember(const QString &program, const QString &arguments)
{
    // Newest first, one entry per program-and-arguments, eight at most.
    for (int i = m_recent.size() - 1; i >= 0; --i)
        if (m_recent.at(i).program.compare(program, Qt::CaseInsensitive) == 0
            && m_recent.at(i).arguments == arguments)
            m_recent.removeAt(i);
    m_recent.prepend({program, arguments});
    while (m_recent.size() > RecentLimit)
        m_recent.removeLast();
    saveRecent(m_recent);
    rebuildRecent();
}

void TiLauncherPage::rebuildRecent()
{
    for (SettingRow *row : std::as_const(m_recentRows)) {
        row->hide();
        row->deleteLater();
    }
    m_recentRows.clear();

    for (const Recent &r : std::as_const(m_recent)) {
        auto *button = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("ti.launch")), m_recentList);
        button->setEnabledLook(m_available);
        const QString desc = r.arguments.isEmpty()
                                 ? r.program
                                 : QStringLiteral("%1 · %2").arg(r.program, r.arguments);
        auto *row = new SettingRow(QFileInfo(r.program).fileName(), desc, button,
                                   SettingRow::Trailing, m_recentList);
        const QString program = r.program;
        const QString arguments = r.arguments;
        connect(button, &PillButton::clicked, this,
                [this, program, arguments, row] { launch(program, arguments, row); });
        m_recentLayout->addWidget(row);
        row->show();
        m_recentRows.append(row);
    }

    m_recentHeader->setCount(Locale::tr(QStringLiteral("ti.recent.count")).arg(m_recent.size()));
    m_recentBlock->setVisible(!m_recent.isEmpty());
}

void TiLauncherPage::retranslate()
{
    m_introHeader->setTitle(Locale::tr(QStringLiteral("ti.title")));
    refreshAvailability();   // the intro text is availability-dependent
    m_targetHeader->setTitle(Locale::tr(QStringLiteral("ti.target.header")));
    m_pathLabel->setText(Locale::tr(QStringLiteral("ti.target.path")));
    m_path->setPlaceholderText(Locale::tr(QStringLiteral("ti.target.placeholder")));
    m_browse->setText(Locale::tr(QStringLiteral("ti.target.browse")));
    m_hint->setText(Locale::tr(QStringLiteral("ti.target.hint")));
    m_argsLabel->setText(Locale::tr(QStringLiteral("ti.target.args")));
    m_args->setPlaceholderText(Locale::tr(QStringLiteral("ti.target.argsPlaceholder")));
    m_launch->setText(Locale::tr(QStringLiteral("ti.target.launch")));
    m_recentHeader->setTitle(Locale::tr(QStringLiteral("ti.recent.header")));
    m_recentClear->setText(Locale::tr(QStringLiteral("ti.recent.clear")));
    m_quickHeader->setTitle(Locale::tr(QStringLiteral("ti.quick.header")));
    m_quickHeader->setCount(Locale::tr(QStringLiteral("actions.count")).arg(m_quick.size()));

    for (const Quick &quick : std::as_const(m_quick)) {
        for (const QuickDef &def : Quicks) {
            if (quick.id != QLatin1String(def.id))
                continue;
            quick.row->setName(Locale::tr(QString::fromLatin1(def.nameKey)));
            quick.row->setDesc(Locale::tr(QString::fromLatin1(def.descKey)));
            quick.button->setText(Locale::tr(QStringLiteral("ti.launch")));
        }
    }
}
