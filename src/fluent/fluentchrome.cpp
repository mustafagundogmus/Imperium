#include "fluentchrome.h"
#include "applybar.h"
#include "categorypane.h"
#include "fluentcontent.h"
#include "fluentheader.h"
#include "fluenticons.h"
#include "fluentsearchbox.h"
#include "fluenttitlebar.h"
#include "iconrail.h"
#include "../appstate.h"
#include "../catalog.h"
#include "../i18n.h"
#include "../theme.h"
#include "../views/sidebar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>

FluentChrome::FluentChrome(AppState *state, QObject *parent)
    : Chrome(parent)
    , m_state(state)
{
    buildRails();
}

FluentChrome::~FluentChrome()
{
    // The content column holds the header and the bar; MainWindow has taken the stack
    // back before this runs.
    delete m_content;
    delete m_titleBar;
    delete m_rail;
    delete m_pane;
}

void FluentChrome::buildRails()
{
    // The handoff's six meanings — home, sliders, wrench, package, history, shield — in
    // lucide's drawings of them rather than the prototype's hand-traced ones, which read
    // as a smudge at 18px (the wrench most of all).
    using namespace FluentIcons::Lucide;
    m_rails.clear();
    m_rails.append({QStringLiteral("category.ov"), &House, {QStringLiteral("ov")}});

    Rail tweaks{QStringLiteral("fluent.rail.tweaks"), &Icons::Lucide::SlidersHorizontal, {}};
    for (const Category &c : Catalog::instance().categories())
        if (!c.isOverview())
            tweaks.ids.append(c.id);
    m_rails.append(tweaks);

    m_rails.append({QStringLiteral("sidebar.group.tools"), &Wrench,
                    {Sidebar::actionsId(), Sidebar::cleanerId(), Sidebar::godModeId(),
                     Sidebar::tiLauncherId()}});
    m_rails.append({QStringLiteral("fluent.rail.packages"), &Icons::Lucide::Package,
                    {Sidebar::debloatId(), Sidebar::appsId(), Sidebar::featuresId()}});
    m_rails.append({QStringLiteral("fluent.rail.history"), &History, {Sidebar::journalId()}});
    m_rails.append({QStringLiteral("fluent.rail.security"), &Icons::Lucide::ShieldCheck,
                    {QStringLiteral("sec"), QStringLiteral("priv")}});
    m_settingsRail = {QStringLiteral("sidebar.settings"), &Settings,
                      {Sidebar::settingsId(), Sidebar::aboutId()}};
}

void FluentChrome::build(QWidget *card, QWidget *stack)
{
    // The rail and the pane run the full height of the card, and the title bar spans
    // only the content column — so the top-left corner holds the first rail button and
    // the search box rather than the logo, which sits centred over the content. The
    // rail is not in the layout: a 56px gap is, and the rail is an overlay the chrome
    // places over it (placeRail), so that it can widen over the pane while the pointer
    // is on it without the layout moving everything beside it.
    auto *root = new QHBoxLayout(card);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);

    m_rail = new IconRail(card);
    QVector<IconRail::Entry> entries;
    for (const Rail &r : std::as_const(m_rails))
        entries.append({Locale::tr(r.labelKey), r.glyph});
    m_rail->setEntries(entries, {Locale::tr(m_settingsRail.labelKey), m_settingsRail.glyph});
    root->addSpacing(Theme::Fluent::RailWidth);

    m_pane = new CategoryPane(card);
    root->addWidget(m_pane);

    auto *column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    root->addLayout(column, 1);

    m_titleBar = new FluentTitleBar(card);
    column->addWidget(m_titleBar);

    m_header = new FluentHeader;
    m_bar = new ApplyBar;
    m_content = new FluentContent(m_header, stack, m_bar, card);
    column->addWidget(m_content, 1);
    stack->show();

    for (QWidget *w : {static_cast<QWidget *>(m_titleBar), static_cast<QWidget *>(m_rail),
                       static_cast<QWidget *>(m_pane), static_cast<QWidget *>(m_content)})
        w->show();

    // Above the pane, which it opens over; and told the card's size from now on.
    m_rail->raise();
    card->installEventFilter(this);
    placeRail();

    connect(m_titleBar, &FluentTitleBar::minimizeRequested, this, &Chrome::minimizeRequested);
    connect(m_titleBar, &FluentTitleBar::maximizeToggleRequested, this, &Chrome::maximizeToggleRequested);
    connect(m_titleBar, &FluentTitleBar::closeRequested, this, &Chrome::closeRequested);
    connect(m_rail, &IconRail::activated, this, &FluentChrome::onRailActivated);
    connect(m_pane, &CategoryPane::activated, this, &Chrome::categoryActivated);
    connect(m_pane, &CategoryPane::restorePointRequested, this, &Chrome::restorePointRequested);
    connect(m_pane->search(), &FluentSearchBox::textChanged, this, &Chrome::queryChanged);
    // The handoff's order is Tümü · Değiştirilen · Etkin; AppState's is All, Enabled, Changed.
    connect(m_header, &FluentHeader::filterChanged, this, [this](int index) {
        static const int toFilter[] = {int(Filter::All), int(Filter::Changed), int(Filter::Enabled)};
        Q_EMIT filterChanged(toFilter[qBound(0, index, 2)]);
    });
    connect(m_header, &FluentHeader::profileRequested, this,
            [this] { Q_EMIT categoryActivated(Sidebar::settingsId()); });
    connect(m_bar, &ApplyBar::journalRequested, this,
            [this] { Q_EMIT categoryActivated(Sidebar::journalId()); });
    connect(m_bar, &ApplyBar::revertRequested, this, &Chrome::revertRequested);
    connect(m_bar, &ApplyBar::applyRequested, this, &Chrome::applyRequested);

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &FluentChrome::retranslate);

    static const int fromFilter[] = {0, 2, 1};
    m_header->setFilterIndex(fromFilter[qBound(0, int(m_state->filter()), 2)]);
    setSelected(m_state->selectedCategory());
}

bool FluentChrome::eventFilter(QObject *watched, QEvent *e)
{
    if (e->type() == QEvent::Resize && m_rail && watched == m_rail->parentWidget())
        placeRail();
    return Chrome::eventFilter(watched, e);
}

void FluentChrome::placeRail()
{
    // Inside the card's 1px border, the full height; the width is the rail's own — the
    // closed 56 or however far open it is at the moment.
    const QWidget *card = m_rail->parentWidget();
    m_rail->setGeometry(1, 1, m_rail->width(), card->height() - 2);
}

int FluentChrome::titleBarHeight() const
{
    return m_titleBar ? m_titleBar->height() : 0;
}

void FluentChrome::setMaximized(bool maximized)
{
    m_titleBar->setMaximized(maximized);
    m_bar->setCornerRadius(maximized ? 0.0 : Theme::Fluent::WindowRadius);
}

int FluentChrome::railOf(const QString &id, int preferred) const
{
    const auto holds = [&id](const Rail &r) { return r.ids.contains(id); };
    if (preferred >= 0 && preferred < m_rails.size() && holds(m_rails.at(preferred)))
        return preferred;
    if (preferred == -1 && holds(m_settingsRail))
        return -1;
    for (int i = 0; i < m_rails.size(); ++i)
        if (holds(m_rails.at(i)))
            return i;
    if (holds(m_settingsRail))
        return -1;
    return preferred;   // an id no rail knows: the --category switch; stay where we are
}

QString FluentChrome::railLabel(int index) const
{
    return Locale::tr(index < 0 ? m_settingsRail.labelKey : m_rails.value(index).labelKey);
}

QString FluentChrome::pageLabel(const QString &id) const
{
    if (id == Sidebar::debloatId())     return Locale::tr(QStringLiteral("sidebar.debloat"));
    if (id == Sidebar::godModeId())     return Locale::tr(QStringLiteral("sidebar.godmode"));
    if (id == Sidebar::cleanerId())     return Locale::tr(QStringLiteral("sidebar.cleaner"));
    if (id == Sidebar::actionsId())     return Locale::tr(QStringLiteral("sidebar.actions"));
    if (id == Sidebar::tiLauncherId())  return Locale::tr(QStringLiteral("sidebar.tilauncher"));
    if (id == Sidebar::appsId())        return Locale::tr(QStringLiteral("sidebar.apps"));
    if (id == Sidebar::featuresId())    return Locale::tr(QStringLiteral("sidebar.features"));
    if (id == Sidebar::journalId())     return Locale::tr(QStringLiteral("sidebar.journal"));
    if (id == Sidebar::settingsId())    return Locale::tr(QStringLiteral("sidebar.settings"));
    if (id == Sidebar::aboutId())       return Locale::tr(QStringLiteral("sidebar.about"));
    return Locale::tr(QStringLiteral("category.") + id);
}

void FluentChrome::showRail(int index)
{
    m_currentRail = index;
    m_rail->setSelected(index);

    const Rail &rail = index < 0 ? m_settingsRail : m_rails.at(index);
    QVector<CategoryPane::Item> items;
    const Catalog &catalog = Catalog::instance();
    for (const QString &id : rail.ids) {
        QString count = m_counts.value(id);
        if (const Category *c = catalog.category(id))
            if (c->tweakCount() > 0)
                count = QString::number(c->tweakCount());
        items.append({id, pageLabel(id), count, &FluentIcons::pageGlyph(id)});
    }
    // The tweak rail's pane is the handoff's "Kategoriler"; every other rail's pane is
    // headed by the rail's own name.
    const QString heading = index == 1 ? Locale::tr(QStringLiteral("fluent.pane.categories"))
                                       : railLabel(index);
    m_pane->setItems(heading, items);
}

void FluentChrome::onRailActivated(int index)
{
    const Rail &rail = index < 0 ? m_settingsRail : m_rails.at(index);
    if (rail.ids.isEmpty())
        return;
    const QString remembered = m_lastInRail.value(index);
    Q_EMIT categoryActivated(rail.ids.contains(remembered) ? remembered : rail.ids.first());
}

void FluentChrome::setSelected(const QString &id)
{
    const int rail = railOf(id, m_currentRail);
    const bool railChanged = rail != m_currentRail || m_pane == nullptr || m_selected.isEmpty();
    m_selected = id;
    m_lastInRail.insert(rail, id);
    if (railChanged)
        showRail(rail);
    m_pane->setSelected(id);
}

void FluentChrome::retranslate()
{
    QStringList labels;
    for (const Rail &r : std::as_const(m_rails))
        labels.append(Locale::tr(r.labelKey));
    m_rail->setLabels(labels, Locale::tr(m_settingsRail.labelKey));
    showRail(m_currentRail);
    m_pane->setSelected(m_selected);
}

void FluentChrome::setCategoryCount(const QString &id, const QString &text)
{
    m_counts.insert(id, text);
    m_pane->setCount(id, text);
}

void FluentChrome::setTitle(const QString &title)
{
    // A search spans the catalogue and belongs under Tweakler whatever rail was open.
    const QString parent = m_state->searching() ? Locale::tr(QStringLiteral("fluent.rail.tweaks"))
                                                : railLabel(m_currentRail);
    m_header->setBreadcrumb(parent, title);
    m_header->setTitle(title);
}

void FluentChrome::setSubtitle(const QString &subtitle)
{
    m_subtitle = subtitle;
    composeSubtitle();
}

void FluentChrome::setPendingLabel(const QString &label)
{
    m_pendingLabel = label;
    composeSubtitle();
}

void FluentChrome::composeSubtitle()
{
    // The classic header draws the pending count as a third, accent-coloured string after
    // a subtitle that ends in a "·" to make room for it. This header has one line, so the
    // two are joined — and the separator dropped when there is nothing to follow it.
    QString line = m_subtitle;
    if (!m_pendingLabel.isEmpty()) {
        line += QLatin1Char(' ') + m_pendingLabel;
    } else {
        const QString separator = QStringLiteral(" ·");
        if (line.endsWith(separator))
            line.chop(separator.size());
    }
    m_header->setSubtitle(line);
}

void FluentChrome::setControlsVisible(bool visible)
{
    m_header->setControlsVisible(visible);
}

void FluentChrome::setFilterCounts(int all, int enabled, int changed)
{
    m_header->setFilterCounts(all, changed, enabled);
}

void FluentChrome::setPending(int count)
{
    m_bar->setPending(count);
}

void FluentChrome::setNotice(const QString &text)
{
    m_bar->setNotice(text);
}

QString FluentChrome::searchText() const
{
    return m_pane->search()->text();
}

void FluentChrome::setSearchText(const QString &text)
{
    m_pane->search()->setText(text);
}

void FluentChrome::clearSearch()
{
    m_pane->search()->clearText();
}

void FluentChrome::focusSearch()
{
    m_pane->search()->focusField();
}

void FluentChrome::setSample(const Sample &sample)
{
    m_pane->setSample(sample);
}

void FluentChrome::setRestorePoint(const QString &text)
{
    m_pane->setRestorePoint(text);
}
