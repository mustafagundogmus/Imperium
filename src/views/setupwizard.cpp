#include "setupwizard.h"

#include "../i18n.h"
#include "../theme.h"
#include "../widgets/accentpicker.h"
#include "../widgets/buttons.h"
#include "../widgets/languagepicker.h"
#include "../widgets/themeswitch.h"
#include "../widgets/typefacepicker.h"
#include "titlebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
constexpr int PageCount = 3;
constexpr int PadX = 40;
constexpr int PadTop = 36;
constexpr int PadBottom = 24;
constexpr int TitleGap = 8;      // title to subtitle
constexpr int BodyGap = 28;      // subtitle to the page's control
constexpr int ControlGap = 22;   // between stacked controls on the appearance page
constexpr int CaptionGap = 8;    // caption to its control

QLabel *makeLabel(const QFont &font, const QColor &color, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setFont(font);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, color);
    label->setPalette(pal);
    return label;
}

} // namespace

SetupWizard::SetupWizard(QWidget *parent)
    : FramelessWindow(parent)
{
    setCardMinimumSize({840, 580});
    resize(minimumSize());

    auto *root = new QVBoxLayout(card());
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);

    m_titleBar = new TitleBar(card());
    root->addWidget(m_titleBar);
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &FramelessWindow::toggleMaximize);
    connect(m_titleBar, &TitleBar::closeRequested, this, [this] { Q_EMIT finished(); });
    connect(this, &FramelessWindow::maximizedChanged, m_titleBar, &TitleBar::setMaximized);

    m_stack = new QStackedWidget(card());
    m_stack->addWidget(buildLanguagePage());
    m_stack->addWidget(buildAppearancePage());
    m_stack->addWidget(buildFinishPage());
    root->addWidget(m_stack, 1);

    auto *navRow = new QWidget(card());
    auto *nav = new QHBoxLayout(navRow);
    nav->setContentsMargins(PadX, 0, PadX, PadBottom);
    m_back = new PillButton(PillButton::Ghost, QString(), navRow);
    m_next = new PillButton(PillButton::Accent, QString(), navRow);
    nav->addWidget(m_back);
    nav->addStretch(1);
    nav->addWidget(m_next);
    root->addWidget(navRow);

    connect(m_back, &PillButton::clicked, this, [this] { showPage(m_page - 1); });
    connect(m_next, &PillButton::clicked, this, [this] {
        if (m_page + 1 < PageCount) {
            showPage(m_page + 1);
        } else {
            Locale::markSetupComplete();
            Q_EMIT finished();
        }
    });

    retranslate();
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &SetupWizard::retranslate);

    showPage(0);
}

QWidget *SetupWizard::buildLanguagePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(PadX, PadTop, PadX, 0);
    layout->setSpacing(0);

    m_title1 = makeLabel(Theme::Font::pageTitle(), Theme::Color::TextPrimary(), page);
    m_subtitle1 = makeLabel(Theme::Font::pageSub(), Theme::Color::TextDesc(), page);
    layout->addWidget(m_title1);
    layout->addSpacing(TitleGap);
    layout->addWidget(m_subtitle1);
    layout->addSpacing(BodyGap);

    auto *language = new LanguagePicker(page);
    connect(language, &LanguagePicker::picked, this, [](const QString &id) {
        Locale::setLanguage(id);
    });
    layout->addWidget(language);
    layout->addStretch(1);

    return page;
}

QWidget *SetupWizard::buildAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(PadX, PadTop, PadX, 0);
    layout->setSpacing(0);

    m_title2 = makeLabel(Theme::Font::pageTitle(), Theme::Color::TextPrimary(), page);
    m_subtitle2 = makeLabel(Theme::Font::pageSub(), Theme::Color::TextDesc(), page);
    layout->addWidget(m_title2);
    layout->addSpacing(TitleGap);
    layout->addWidget(m_subtitle2);
    layout->addSpacing(BodyGap);

    m_captionTheme = makeLabel(Theme::Font::infoLabel(), Theme::Color::TextSecondary(), page);
    auto *theme = new ThemeSwitch(page);
    connect(theme, &ThemeSwitch::picked, this, [](Theme::Appearance a) { Theme::setAppearance(a); });
    layout->addWidget(m_captionTheme);
    layout->addSpacing(CaptionGap);
    layout->addWidget(theme, 0, Qt::AlignLeft);
    layout->addSpacing(ControlGap);

    m_captionAccent = makeLabel(Theme::Font::infoLabel(), Theme::Color::TextSecondary(), page);
    auto *accent = new AccentPicker(page);
    connect(accent, &AccentPicker::picked, this, [](const QColor &c) { Theme::setAccent(c); });
    layout->addWidget(m_captionAccent);
    layout->addSpacing(CaptionGap);
    layout->addWidget(accent, 0, Qt::AlignLeft);
    layout->addSpacing(ControlGap);

    m_captionTypeface = makeLabel(Theme::Font::infoLabel(), Theme::Color::TextSecondary(), page);
    auto *typeface = new TypefacePicker(page);
    connect(typeface, &TypefacePicker::picked, this, [](const QString &id) { Theme::setTypeface(id); });
    layout->addWidget(m_captionTypeface);
    layout->addSpacing(CaptionGap);
    layout->addWidget(typeface, 0, Qt::AlignLeft);

    layout->addStretch(1);
    return page;
}

QWidget *SetupWizard::buildFinishPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(PadX, PadTop, PadX, 0);
    layout->setSpacing(0);

    m_title3 = makeLabel(Theme::Font::pageTitle(), Theme::Color::TextPrimary(), page);
    m_subtitle3 = makeLabel(Theme::Font::pageSub(), Theme::Color::TextDesc(), page);
    m_subtitle3->setWordWrap(true);
    layout->addWidget(m_title3);
    layout->addSpacing(TitleGap);
    layout->addWidget(m_subtitle3);
    layout->addStretch(1);

    return page;
}

void SetupWizard::showPage(int index)
{
    m_page = qBound(0, index, PageCount - 1);
    m_stack->setCurrentIndex(m_page);
    m_back->setVisible(m_page > 0);
    m_next->setText(Locale::tr(m_page + 1 < PageCount ? QStringLiteral("setup.nav.next")
                                                       : QStringLiteral("setup.nav.finish")));
}

void SetupWizard::retranslate()
{
    m_title1->setText(Locale::tr(QStringLiteral("setup.page1.title")));
    m_subtitle1->setText(Locale::tr(QStringLiteral("setup.page1.subtitle")));
    m_title2->setText(Locale::tr(QStringLiteral("setup.page2.title")));
    m_subtitle2->setText(Locale::tr(QStringLiteral("setup.page2.subtitle")));
    m_subtitle2->setWordWrap(true);
    m_title3->setText(Locale::tr(QStringLiteral("setup.page3.title")));
    m_subtitle3->setText(Locale::tr(QStringLiteral("setup.page3.subtitle")));

    m_captionTheme->setText(Locale::tr(QStringLiteral("settings.theme.label")));
    m_captionAccent->setText(Locale::tr(QStringLiteral("settings.accent.label")));
    m_captionTypeface->setText(Locale::tr(QStringLiteral("settings.typeface.label")));

    m_back->setText(Locale::tr(QStringLiteral("setup.nav.back")));
    m_next->setText(Locale::tr(m_page + 1 < PageCount ? QStringLiteral("setup.nav.next")
                                                       : QStringLiteral("setup.nav.finish")));
}
