#include "tilauncherpage.h"

#include "../i18n.h"
#include "../theme.h"
#include "../trustedinstaller.h"
#include "../widgets/buttons.h"
#include "../widgets/sectionheader.h"
#include "../widgets/settingrow.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <iterator>

namespace {

/// The tools worth reaching for at this level, resolved to absolute paths so the launcher
/// finds them without a PATH search. Everything else the user browses to by hand.
struct QuickDef
{
    const char *id;
    const char *relative;   ///< under %SystemRoot%
    const char *nameKey;
    const char *descKey;
};

constexpr QuickDef Quicks[] = {
    {"cmd",        "System32\\cmd.exe",                            "ti.quick.cmd.name",      "ti.quick.cmd.desc"},
    {"powershell", "System32\\WindowsPowerShell\\v1.0\\powershell.exe", "ti.quick.powershell.name", "ti.quick.powershell.desc"},
    {"regedit",    "regedit.exe",                                  "ti.quick.regedit.name",  "ti.quick.regedit.desc"},
    {"explorer",   "explorer.exe",                                 "ti.quick.explorer.name", "ti.quick.explorer.desc"},
};

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

} // namespace

TiLauncherPage::TiLauncherPage(QWidget *parent)
    : QWidget(parent)
{
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
        pathRow->addWidget(m_path, 1);
        m_browse = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("ti.target.browse")), block);
        connect(m_browse, &PillButton::clicked, this, &TiLauncherPage::browse);
        pathRow->addWidget(m_browse, 0);
        layout->addLayout(pathRow);

        m_argsLabel = makeCaption(Locale::tr(QStringLiteral("ti.target.args")), block);
        layout->addSpacing(4);
        layout->addWidget(m_argsLabel);
        m_args = new QLineEdit(block);
        m_args->setPlaceholderText(Locale::tr(QStringLiteral("ti.target.argsPlaceholder")));
        m_args->setFont(Theme::Font::tweakName());
        m_args->setFixedHeight(Theme::Metric::SearchHeight);
        layout->addWidget(m_args);

        auto *launchRow = new QHBoxLayout;
        launchRow->setContentsMargins(0, 0, 0, 0);
        launchRow->addStretch(1);
        m_launch = new PillButton(PillButton::Accent, Locale::tr(QStringLiteral("ti.target.launch")), block);
        connect(m_launch, &PillButton::clicked, this, &TiLauncherPage::launchFromField);
        connect(m_path, &QLineEdit::returnPressed, this, &TiLauncherPage::launchFromField);
        launchRow->addWidget(m_launch, 0);
        layout->addSpacing(2);
        layout->addLayout(launchRow);

        outer->addWidget(block);
    }

    // --- quick launches ----------------------------------------------------
    {
        auto *block = new QWidget(this);
        auto *layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_quickHeader = new SectionHeader(Locale::tr(QStringLiteral("ti.quick.header")), block);
        m_quickHeader->setCount(Locale::tr(QStringLiteral("actions.count"))
                                    .arg(int(std::size(Quicks))));
        layout->addWidget(m_quickHeader);

        auto *list = new QWidget(block);
        auto *listLayout = new QVBoxLayout(list);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(1);

        const QString root = systemRoot();
        for (const QuickDef &def : Quicks) {
            Quick quick;
            quick.id = QString::fromLatin1(def.id);
            quick.program = root + QLatin1Char('\\') + QString::fromLatin1(def.relative);

            quick.button = new PillButton(PillButton::Ghost,
                                          Locale::tr(QStringLiteral("ti.launch")), list);
            SettingRow *row = new SettingRow(Locale::tr(QString::fromLatin1(def.nameKey)),
                                             Locale::tr(QString::fromLatin1(def.descKey)),
                                             quick.button, SettingRow::Trailing, list);
            quick.row = row;

            const QString program = quick.program;
            connect(quick.button, &PillButton::clicked, this,
                    [this, program, row] { launch(program, QString(), row); });

            listLayout->addWidget(row);
            m_quick.append(quick);
        }

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
    for (const Quick &quick : std::as_const(m_quick))
        quick.button->setEnabledLook(m_available);
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
    launch(target, m_args->text().trimmed(), nullptr);
}

void TiLauncherPage::launch(const QString &program, const QString &arguments, SettingRow *row)
{
    if (!m_available) {
        Q_EMIT notice(Locale::tr(QStringLiteral("ti.err.notElevated")));
        return;
    }

    const TrustedInstaller::Result result =
        TrustedInstaller::launch(program, arguments, QString());

    const QString message = result.ok
                                ? Locale::tr(QStringLiteral("ti.result.launched"))
                                      .arg(QFileInfo(program).fileName())
                                      .arg(result.pid)
                                : (result.detail.isEmpty()
                                       ? result.summary
                                       : QStringLiteral("%1 · %2").arg(result.summary, result.detail));

    if (row)
        row->setDesc(message);
    Q_EMIT notice(result.ok ? Locale::tr(QStringLiteral("ti.result.notice"))
                                  .arg(QFileInfo(program).fileName())
                            : message);
}

void TiLauncherPage::retranslate()
{
    m_introHeader->setTitle(Locale::tr(QStringLiteral("ti.title")));
    refreshAvailability();   // the intro text is availability-dependent
    m_targetHeader->setTitle(Locale::tr(QStringLiteral("ti.target.header")));
    m_pathLabel->setText(Locale::tr(QStringLiteral("ti.target.path")));
    m_path->setPlaceholderText(Locale::tr(QStringLiteral("ti.target.placeholder")));
    m_browse->setText(Locale::tr(QStringLiteral("ti.target.browse")));
    m_argsLabel->setText(Locale::tr(QStringLiteral("ti.target.args")));
    m_args->setPlaceholderText(Locale::tr(QStringLiteral("ti.target.argsPlaceholder")));
    m_launch->setText(Locale::tr(QStringLiteral("ti.target.launch")));
    m_quickHeader->setTitle(Locale::tr(QStringLiteral("ti.quick.header")));

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
