#include "aboutpage.h"

#include "../i18n.h"
#include "../theme.h"
#include "../updater.h"
#include "../widgets/buttons.h"
#include "../widgets/settingrow.h"

#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// buymeacoffee.com takes a page slug, not an @handle — no leading "@" here.
const QString DonateUrl = QStringLiteral("https://buymeacoffee.com/berkayay");

} // namespace

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                              Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    outer->setSpacing(1);

    auto *profile = new PillButton(PillButton::Ghost, QString(), this);
    connect(profile, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/shadesofdeath")));
    });
    m_developerRow = new SettingRow(QString(), QString(), profile, SettingRow::Trailing, this);

    auto *repo = new PillButton(PillButton::Ghost, QString(), this);
    connect(repo, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/%1").arg(Updater::repository())));
    });
    m_sourceRow = new SettingRow(QString(), QString(), repo, SettingRow::Trailing, this);

    auto *issues = new PillButton(PillButton::Ghost, QString(), this);
    connect(issues, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/%1/issues").arg(Updater::repository())));
    });
    m_issuesRow = new SettingRow(QString(), QString(), issues, SettingRow::Trailing, this);

    // The one row here that is not about the project itself. Kept last, and kept a plain
    // ghost pill like the others rather than dressed up — asking is enough on its own.
    auto *donate = new PillButton(PillButton::Ghost, QString(), this);
    connect(donate, &PillButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(DonateUrl));
    });
    m_donateRow = new SettingRow(QString(), QString(), donate, SettingRow::Trailing, this);

    for (SettingRow *row : {m_developerRow, m_sourceRow, m_issuesRow, m_donateRow}) {
        outer->addWidget(row);
        ++m_rowCount;
    }
    outer->addStretch(1);

    retranslate();
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &AboutPage::retranslate);
}

void AboutPage::retranslate()
{
    m_developerRow->setName(Locale::tr(QStringLiteral("settings.about.developer.label")));
    m_developerRow->setDesc(Locale::tr(QStringLiteral("settings.about.developer.desc")));

    m_sourceRow->setName(Locale::tr(QStringLiteral("settings.about.source.label")));
    m_sourceRow->setDesc(Locale::tr(QStringLiteral("settings.about.source.desc"))
                             .arg(Updater::repository()));

    m_issuesRow->setName(Locale::tr(QStringLiteral("settings.about.issues.label")));
    m_issuesRow->setDesc(Locale::tr(QStringLiteral("settings.about.issues.desc")));

    m_donateRow->setName(Locale::tr(QStringLiteral("about.donate.label")));
    m_donateRow->setDesc(Locale::tr(QStringLiteral("about.donate.desc")));

    // The buttons are the trailing controls SettingRow owns, not this page — reach them
    // through the rows rather than keeping four more member pointers around for it.
    static_cast<PillButton *>(m_developerRow->findChild<PillButton *>())
        ->setText(Locale::tr(QStringLiteral("settings.about.developer.button")));
    static_cast<PillButton *>(m_sourceRow->findChild<PillButton *>())
        ->setText(Locale::tr(QStringLiteral("settings.repo.open")));
    static_cast<PillButton *>(m_issuesRow->findChild<PillButton *>())
        ->setText(Locale::tr(QStringLiteral("settings.about.issues.button")));
    static_cast<PillButton *>(m_donateRow->findChild<PillButton *>())
        ->setText(Locale::tr(QStringLiteral("about.donate.button")));
}
