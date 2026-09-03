#include "classicchrome.h"
#include "contentheader.h"
#include "sidebar.h"
#include "statusbar.h"
#include "titlebar.h"
#include "../widgets/searchfield.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

ClassicChrome::ClassicChrome(AppState *state, QObject *parent)
    : Chrome(parent)
    , m_state(state)
{
}

ClassicChrome::~ClassicChrome()
{
    delete m_titleBar;
    delete m_sidebar;
    delete m_header;
    delete m_statusBar;
}

void ClassicChrome::build(QWidget *card, QWidget *stack)
{
    auto *root = new QVBoxLayout(card);
    root->setContentsMargins(1, 1, 1, 1);   // the card's own 1px border
    root->setSpacing(0);

    m_titleBar = new TitleBar(card);
    root->addWidget(m_titleBar);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    root->addLayout(row, 1);

    m_sidebar = new Sidebar(m_state, card);
    row->addWidget(m_sidebar);

    auto *main = new QVBoxLayout;
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    row->addLayout(main, 1);

    m_header = new ContentHeader(card);
    main->addWidget(m_header);

    stack->setParent(card);
    main->addWidget(stack, 1);
    stack->show();

    m_statusBar = new StatusBar(card);
    main->addWidget(m_statusBar);

    for (QWidget *w : {static_cast<QWidget *>(m_titleBar), static_cast<QWidget *>(m_sidebar),
                       static_cast<QWidget *>(m_header), static_cast<QWidget *>(m_statusBar)})
        w->show();

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &Chrome::minimizeRequested);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &Chrome::maximizeToggleRequested);
    connect(m_titleBar, &TitleBar::closeRequested, this, &Chrome::closeRequested);
    connect(m_sidebar, &Sidebar::categoryActivated, this, &Chrome::categoryActivated);
    connect(m_sidebar->search(), &SearchField::textChanged, this, &Chrome::queryChanged);
    // The classic filter is in AppState's own order: Tümü, Etkin, Değişen.
    connect(m_header, &ContentHeader::filterChanged, this, &Chrome::filterChanged);
    connect(m_header, &ContentHeader::sortToggled, this, &Chrome::sortToggled);
    connect(m_statusBar, &StatusBar::applyRequested, this, &Chrome::applyRequested);
    connect(m_statusBar, &StatusBar::revertRequested, this, &Chrome::revertRequested);
}

int ClassicChrome::titleBarHeight() const
{
    return m_titleBar ? m_titleBar->height() : 0;
}

void ClassicChrome::setMaximized(bool maximized)
{
    m_titleBar->setMaximized(maximized);
}

void ClassicChrome::setSystemSummary(const QString &summary)
{
    m_titleBar->setSystemSummary(summary);
}

void ClassicChrome::setSelected(const QString &id)
{
    m_sidebar->setSelected(id);
}

void ClassicChrome::setCategoryCount(const QString &id, const QString &text)
{
    m_sidebar->setCategoryCount(id, text);
}

void ClassicChrome::setTitle(const QString &title)
{
    m_header->setTitle(title);
}

void ClassicChrome::setSubtitle(const QString &subtitle)
{
    m_header->setSubtitle(subtitle);
}

void ClassicChrome::setPendingLabel(const QString &label)
{
    m_header->setPendingLabel(label);
}

void ClassicChrome::setControlsVisible(bool visible)
{
    m_header->setControlsVisible(visible);
}

void ClassicChrome::setPending(int count)
{
    m_statusBar->setPending(count);
}

void ClassicChrome::setSummary(const QString &summary)
{
    m_statusBar->setSummary(summary);
}

void ClassicChrome::setNotice(const QString &text)
{
    m_statusBar->setNotice(text);
}

QString ClassicChrome::searchText() const
{
    return m_sidebar->search()->text();
}

void ClassicChrome::setSearchText(const QString &text)
{
    m_sidebar->search()->setText(text);
}

void ClassicChrome::clearSearch()
{
    m_sidebar->search()->clearText();
}

void ClassicChrome::focusSearch()
{
    m_sidebar->search()->focusField();
}
