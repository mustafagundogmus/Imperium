#include "aboutpage.h"

#include "../action.h"
#include "../catalog.h"
#include "../i18n.h"
#include "../icons.h"
#include "../theme.h"
#include "../updater.h"
#include "../widgets/buttons.h"
#include "../widgets/overviewblocks.h"
#include "../widgets/settingrow.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QGridLayout>
#include <QUrl>
#include <QVBoxLayout>

#include <iterator>

namespace {

// buymeacoffee.com takes a page slug, not an @handle — no leading "@" here.
const QString DonateUrl = QStringLiteral("https://buymeacoffee.com/berkayay");

// The gutter between the two cards, the same 12 the Genel Bakış grid puts between its
// columns — these are that grid's cards and would read as a different component if they
// sat at a different distance from each other.
constexpr int CardGap = 12;

} // namespace

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                              Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    outer->setSpacing(Theme::Metric::SectionGap);

    // --- the numbers -------------------------------------------------------
    //
    // Not a feature list: every figure below is read back out of the running build, so the
    // page is incapable of advertising a tweak, a language or a typeface that this
    // executable does not actually carry.
    {
        auto *cards = new QWidget(this);
        auto *grid = new QGridLayout(cards);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(CardGap);
        grid->setVerticalSpacing(CardGap);

        // The application's own name, which is not a translated string in any language.
        m_appCard = new InfoSection(QStringLiteral("Arbitrium"),
                                    Icons::Lucide::SlidersHorizontal, cards);
        // Dil sits with the look rather than with the build because that is where Ayarlar
        // files it too — one section, "Görünüm", holding theme, language, face and size.
        m_lookCard = new InfoSection(Locale::tr(QStringLiteral("settings.section.appearance")),
                                     Icons::Lucide::SunMoon, cards);

        grid->addWidget(m_appCard, 0, 0);
        grid->addWidget(m_lookCard, 0, 1);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);

        outer->addWidget(cards);
    }

    // --- where to go next --------------------------------------------------
    //
    // No section header over these four. The content header is already announcing the page
    // as "Geliştirici, kaynak kod ve destek", which is the list, and a heading repeating
    // the subtitle two lines under it is a heading that says nothing.
    {
        auto *list = new QWidget(this);
        auto *listLayout = new QVBoxLayout(list);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(1);

        auto *profile = new PillButton(PillButton::Ghost, QString(), list);
        connect(profile, &PillButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/mustafagundogmus")));
        });
        m_developerRow = new SettingRow(QString(), QString(), profile, SettingRow::Trailing, list);

        auto *repo = new PillButton(PillButton::Ghost, QString(), list);
        connect(repo, &PillButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/%1").arg(Updater::repository())));
        });
        m_sourceRow = new SettingRow(QString(), QString(), repo, SettingRow::Trailing, list);

        auto *issues = new PillButton(PillButton::Ghost, QString(), list);
        connect(issues, &PillButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/%1/issues").arg(Updater::repository())));
        });
        m_issuesRow = new SettingRow(QString(), QString(), issues, SettingRow::Trailing, list);

        // The one row here that is not about the project itself. Kept last, and kept a plain
        // ghost pill like the others rather than dressed up — asking is enough on its own.
        auto *donate = new PillButton(PillButton::Ghost, QString(), list);
        connect(donate, &PillButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl(DonateUrl));
        });
        m_donateRow = new SettingRow(QString(), QString(), donate, SettingRow::Trailing, list);

        for (SettingRow *row : {m_developerRow, m_sourceRow, m_issuesRow, m_donateRow}) {
            listLayout->addWidget(row);
            ++m_rowCount;
        }

        outer->addWidget(list);
    }

    outer->addStretch(1);

    retranslate();
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &AboutPage::retranslate);
    // Nothing else to hook up: both cards are InfoSections, which repaint from the tokens
    // inside their own paintEvent and resize themselves on typefaceChanged, and MainWindow
    // already updates the whole tree when the appearance changes.
}

void AboutPage::retranslate()
{
    // --- the numbers, read back from the build ------------------------------
    m_appCard->setRows({
        {Locale::tr(QStringLiteral("settings.version.label")),
         QCoreApplication::applicationVersion(), true},
        {QStringLiteral("Qt"), QStringLiteral(QT_VERSION_STR), true},
        // The one value here that is a name rather than a number, and the only reason the
        // page needs a label of its own: MIT is what LICENSE at the repository root says.
        {Locale::tr(QStringLiteral("about.license")), QStringLiteral("MIT"), false},
        {Locale::tr(QStringLiteral("ov.katalog")),
         Locale::tr(QStringLiteral("ov.tweakSayisi")).arg(Catalog::instance().totalTweaks()), true},
        {Locale::tr(QStringLiteral("sidebar.actions")),
         Locale::tr(QStringLiteral("actions.count")).arg(ActionCatalog::instance().total()), true},
    });

    // Four of the five come straight off the tables that define them, so a seventh face or
    // a ninth accent updates this card by doing nothing to it. The fifth cannot: a C++ enum
    // has no count, so the themes go through Theme::AppearanceCount, and the static_assert
    // beside it is what stops that number from quietly saying eight forever.
    m_lookCard->setTitle(Locale::tr(QStringLiteral("settings.section.appearance")));
    m_lookCard->setRows({
        {Locale::tr(QStringLiteral("settings.theme.label")),
         QString::number(Theme::AppearanceCount), true},
        {Locale::tr(QStringLiteral("settings.accent.label")),
         QString::number(Theme::accentPresets().size()), true},
        {Locale::tr(QStringLiteral("settings.language.label")),
         QString::number(Locale::languages().size()), true},
        {Locale::tr(QStringLiteral("settings.typeface.label")),
         QString::number(Theme::typefaces().size()), true},
        {Locale::tr(QStringLiteral("settings.fontsize.label")),
         QString::number(int(std::size(Theme::FontScaleSteps))), true},
    });

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
