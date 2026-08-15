#include "mainwindow.h"

#include "appstate.h"
#include "catalog.h"
#include "theme.h"
#include "tweakengine.h"
#include "views/contentheader.h"
#include "views/overviewpage.h"
#include "views/sidebar.h"
#include "settings.h"
#include "updater.h"
#include "views/settingspage.h"
#include "views/statusbar.h"
#include "widgets/applyoverlay.h"
#include "views/titlebar.h"
#include "views/tweakpage.h"
#include "widgets/searchfield.h"
#include "widgets/smoothscrollarea.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QProcess>
#include <QShortcut>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

bool matches(const Tweak &t, const QString &needle)
{
    return t.name.contains(needle, Qt::CaseInsensitive)
           || t.desc.contains(needle, Qt::CaseInsensitive);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowTitle(QCoreApplication::applicationName());

    m_engine = new TweakEngine(this);
    m_state = new AppState(m_engine, this);
    m_facts = SysInfo::collect();
    m_scannedAt = QDateTime::currentDateTime();
    m_monitor = new SystemMonitor(this);

    buildUi();
    wire();

    setCardMinimumSize({Theme::Metric::WindowWidth, Theme::Metric::WindowHeight});
    resize(minimumSize());

    const int total = Catalog::instance().totalTweaks();
    m_titleBar->setSystemSummary(m_facts.titleBarSummary);
    m_statusBar->setSummary(total > 0
                                ? QStringLiteral("%1/%1 tweak yüklendi · profil: varsayılan").arg(total)
                                : QStringLiteral("katalog boş · profil: varsayılan"));
    m_overview->setFacts(m_facts);
    m_monitor->start();

    refreshView();
    refreshCounters();

    m_probe = new SysInfo::Probe(this);
    connect(m_probe, &SysInfo::Probe::resolved, this,
            [this](const QString &activation, const QString &restorePoint, const QString &hotfix) {
                if (!activation.isEmpty())
                    m_facts.activation = activation;
                if (!restorePoint.isEmpty())
                    m_facts.lastRestorePoint = restorePoint;
                if (!hotfix.isEmpty())
                    m_facts.lastUpdate = hotfix;
                m_overview->setFacts(m_facts);
                m_sidebar->setRestorePoint(restorePoint.isEmpty() ? QStringLiteral("Yok")
                                                                  : restorePoint);
            });
    m_probe->start();
}

void MainWindow::buildUi()
{
    auto *root = new QVBoxLayout(card());
    root->setContentsMargins(1, 1, 1, 1);   // the card's own 1px border
    root->setSpacing(0);

    m_titleBar = new TitleBar(card());
    root->addWidget(m_titleBar);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    root->addLayout(row, 1);

    m_sidebar = new Sidebar(m_state, card());
    row->addWidget(m_sidebar);

    auto *main = new QVBoxLayout;
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    row->addLayout(main, 1);

    m_header = new ContentHeader(card());
    main->addWidget(m_header);

    m_stack = new QStackedWidget(card());

    m_overviewScroll = new SmoothScrollArea(m_stack);
    m_overview = new OverviewPage(m_monitor, m_overviewScroll);
    m_overviewScroll->setWidget(m_overview);

    m_tweakScroll = new SmoothScrollArea(m_stack);
    m_tweaks = new TweakPage(m_state, m_tweakScroll);
    m_tweakScroll->setWidget(m_tweaks);

    m_updater = new Updater(this);
    m_settingsScroll = new SmoothScrollArea(m_stack);
    m_settings = new SettingsPage(m_state, m_updater, m_settingsScroll);
    m_settingsScroll->setWidget(m_settings);

    m_stack->addWidget(m_overviewScroll);
    m_stack->addWidget(m_tweakScroll);
    m_stack->addWidget(m_settingsScroll);
    main->addWidget(m_stack, 1);

    m_statusBar = new StatusBar(card());
    main->addWidget(m_statusBar);

    // Sits above the content column, not in a layout: it covers the header, the list and
    // the status bar while a write is running, and leaves the title bar reachable.
    m_applyOverlay = new ApplyOverlay(m_state, card());
    m_applyOverlay->hide();
}

void MainWindow::wire()
{
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &FramelessWindow::toggleMaximize);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(this, &FramelessWindow::maximizedChanged, m_titleBar, &TitleBar::setMaximized);

    connect(m_sidebar, &Sidebar::categoryActivated, this, &MainWindow::onCategoryActivated);
    connect(m_sidebar, &Sidebar::restorePointRequested, this, &MainWindow::onRestorePointRequested);
    connect(m_sidebar->search(), &SearchField::textChanged, this, &MainWindow::onQueryChanged);

    connect(m_header, &ContentHeader::filterChanged, this, &MainWindow::onFilterChanged);
    connect(m_header, &ContentHeader::sortToggled, this, &MainWindow::onSortToggled);

    connect(m_statusBar, &StatusBar::applyRequested, this, &MainWindow::onApply);
    connect(m_applyOverlay, &ApplyOverlay::finished, this, &MainWindow::onApplyFinished);
    connect(m_applyOverlay, &ApplyOverlay::notice, m_statusBar, &StatusBar::setNotice);
    connect(m_statusBar, &StatusBar::revertRequested, this, &MainWindow::onRevert);

    connect(m_state, &AppState::pendingChanged, this, &MainWindow::refreshCounters);
    connect(m_state, &AppState::committed, this, [this] {
        refreshCounters();
        refreshView();
    });

    connect(m_settings, &SettingsPage::notice, m_statusBar, &StatusBar::setNotice);
    connect(m_settings, &SettingsPage::presetApplied, this,
            [this](const QString &name, int changed, int unknown) {
                QString text = QStringLiteral("“%1” yüklendi · %2 anahtar değişti")
                                   .arg(name).arg(changed);
                if (unknown > 0)
                    text += QStringLiteral(" · %1 bilinmeyen atlandı").arg(unknown);
                m_statusBar->setNotice(text);
                refreshView();
            });

    // Every widget paints its own colours, so a palette swap means a full repaint.
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, [this] {
        const QList<QWidget *> all = findChildren<QWidget *>();
        for (QWidget *w : all)
            w->update();
        update();
    });

    auto *focusSearch = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this);
    connect(focusSearch, &QShortcut::activated, this, [this] { m_sidebar->search()->focusField(); });

    auto *clearSearch = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(clearSearch, &QShortcut::activated, this, [this] { m_sidebar->search()->clearText(); });
}

QVector<Section> MainWindow::visibleSections() const
{
    const Catalog &catalog = Catalog::instance();
    const Filter filter = m_state->filter();

    const auto keep = [this, filter](const Tweak &t) {
        switch (filter) {
        case Filter::Enabled: return m_state->isOn(t.id);
        case Filter::Changed: return m_state->isPending(t.id);
        case Filter::All:     break;
        }
        return true;
    };

    QVector<Section> result;

    if (m_state->searching()) {
        // Search spans the whole catalogue; each category becomes one section.
        const QString needle = m_state->query().trimmed();
        for (const Category &c : catalog.categories()) {
            Section hits;
            hits.title = c.name;
            for (const Section &s : c.sections)
                for (const Tweak &t : s.tweaks)
                    if (keep(t) && matches(t, needle))
                        hits.tweaks.append(t);
            if (!hits.tweaks.isEmpty())
                result.append(hits);
        }
    } else {
        const Category *c = catalog.category(m_state->selectedCategory());
        if (!c)
            return result;
        for (const Section &s : c->sections) {
            Section kept;
            kept.title = s.title;
            for (const Tweak &t : s.tweaks)
                if (keep(t))
                    kept.tweaks.append(t);
            if (!kept.tweaks.isEmpty())
                result.append(kept);
        }
    }

    if (m_alphabetical) {
        for (Section &s : result)
            std::sort(s.tweaks.begin(), s.tweaks.end(), [](const Tweak &a, const Tweak &b) {
                return a.name.localeAwareCompare(b.name) < 0;
            });
    }

    return result;
}

void MainWindow::refreshView()
{
    const Catalog &catalog = Catalog::instance();
    const Category *category = catalog.category(m_state->selectedCategory());
    const bool searching = m_state->searching();
    const bool settings = !searching && m_state->selectedCategory() == Sidebar::settingsId();
    const bool overview = !searching && category && category->isOverview();

    m_header->setControlsVisible(!overview && !settings);

    if (settings) {
        m_header->setTitle(QStringLiteral("Ayarlar"));
        m_header->setSubtitle(QStringLiteral("%1 seçenek · sürüm %2")
                                  .arg(m_settings->rowCount())
                                  .arg(QCoreApplication::applicationVersion()));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_settingsScroll);
        return;
    }

    if (overview) {
        m_header->setTitle(category->name);
        m_header->setSubtitle(QStringLiteral("%1 · son tarama: %2")
                                  .arg(m_facts.computerName,
                                       SysInfo::friendlyDateTime(m_scannedAt).toLower()));
        m_header->setPendingLabel({});
        m_stack->setCurrentWidget(m_overviewScroll);
        return;
    }

    const QVector<Section> sections = visibleSections();
    QString emptyMessage;

    if (searching) {
        int hits = 0;
        for (const Section &s : sections)
            hits += s.tweaks.size();
        m_header->setTitle(QStringLiteral("Arama"));
        m_header->setSubtitle(QStringLiteral("“%1” · %2 sonuç ·").arg(m_state->query().trimmed()).arg(hits));
        m_header->setPendingLabel(QStringLiteral("%1 bekliyor").arg(m_state->pendingCount()));
    } else if (category->tweakCount() == 0) {
        // The catalogue is being rebuilt one page at a time; an empty category says so
        // rather than offering filters over nothing.
        m_header->setTitle(category->name);
        m_header->setSubtitle(QStringLiteral("henüz tweak eklenmedi"));
        m_header->setPendingLabel({});
        m_header->setControlsVisible(false);
        emptyMessage = QStringLiteral("Bu sayfa henüz doldurulmadı.");
    } else {
        m_header->setTitle(category->name);
        m_header->setSubtitle(QStringLiteral("%1 tweak · %2 etkin ·")
                                  .arg(category->tweakCount())
                                  .arg(m_state->appliedCount(*category)));
        m_header->setPendingLabel(QStringLiteral("%1 bekliyor").arg(m_state->pendingCount(*category)));
    }

    m_tweaks->setSections(sections, emptyMessage);
    m_stack->setCurrentWidget(m_tweakScroll);
}

void MainWindow::refreshCounters()
{
    m_statusBar->setPending(m_state->pendingCount());

    const Category *category = Catalog::instance().category(m_state->selectedCategory());
    if (m_state->selectedCategory() == Sidebar::settingsId())
        m_header->setPendingLabel({});
    else if (m_state->searching())
        m_header->setPendingLabel(QStringLiteral("%1 bekliyor").arg(m_state->pendingCount()));
    else if (category && !category->isOverview())
        m_header->setPendingLabel(QStringLiteral("%1 bekliyor").arg(m_state->pendingCount(*category)));
}

void MainWindow::showCategory(const QString &id)
{
    if (Catalog::instance().category(id) || id == Sidebar::settingsId())
        onCategoryActivated(id);
}

void MainWindow::onCategoryActivated(const QString &id)
{
    // Picking a category is an explicit "show me this", so it leaves the search behind.
    if (m_state->searching())
        m_sidebar->search()->clearText();

    m_state->setSelectedCategory(id);
    m_sidebar->setSelected(id);
    m_tweakScroll->scrollToTop();
    m_overviewScroll->scrollToTop();
    refreshView();
}

void MainWindow::onQueryChanged(const QString &query)
{
    m_state->setQuery(query);
    m_tweakScroll->scrollToTop();
    refreshView();
}

void MainWindow::onFilterChanged(int index)
{
    m_state->setFilter(static_cast<Filter>(index));
    m_tweakScroll->scrollToTop();
    refreshView();
}

void MainWindow::onSortToggled(bool alphabetical)
{
    m_alphabetical = alphabetical;
    refreshView();
}

QRect MainWindow::overlayRect() const
{
    // Everything below the title bar, sidebar included: while a write is running the
    // user should not be able to navigate away from it. The title bar stays reachable
    // so the window can still be moved or minimised.
    const QRect inner = card()->rect().adjusted(1, 1, -1, -1);
    return inner.adjusted(0, m_titleBar->height(), 0, 0);
}

void MainWindow::onApply()
{
    if (m_state->pendingCount() == 0 || m_applyOverlay->running())
        return;

    // The overlay drives the writes itself, one per tick, and reports back when done.
    m_applyOverlay->setGeometry(overlayRect());
    m_applyOverlay->run();
}

void MainWindow::onApplyFinished(int succeeded, int failed, bool elevationRequired)
{
    refreshCounters();
    refreshView();

    if (failed == 0) {
        if (succeeded > 0)
            m_statusBar->setNotice(QStringLiteral("%1 tweak uygulandı").arg(succeeded));
        return;
    }

    const AppState::ApplyReport report{succeeded, failed, elevationRequired, QString()};

    if (report.elevationRequired && !TweakEngine::isElevated()) {
        // These tweaks live outside HKCU, so they cannot be written by a standard token.
        QMessageBox box(this);
        box.setWindowTitle(QCoreApplication::applicationName());
        box.setIcon(QMessageBox::NoIcon);
        box.setText(QStringLiteral("%1 tweak yönetici yetkisi gerektiriyor.").arg(report.failed));
        box.setInformativeText(QStringLiteral(
            "Uygulamayı yönetici olarak yeniden başlatayım mı? Beklemedeki anahtarlar "
            "korunur ve yeniden başladıktan sonra yeniden uygulayabilirsiniz."));
        QAbstractButton *restart = box.addButton(QStringLiteral("Yönetici olarak başlat"),
                                                 QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Şimdilik kalsın"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == restart) {
            m_state->stashPending();
            if (TweakEngine::relaunchElevated())
                close();
            else
                m_statusBar->setNotice(QStringLiteral("yeniden başlatma iptal edildi"));
        } else {
            m_statusBar->setNotice(QStringLiteral("%1 tweak yönetici yetkisi bekliyor").arg(report.failed));
        }
        return;
    }

    m_statusBar->setNotice(report.firstError.isEmpty()
                               ? QStringLiteral("%1 tweak uygulanamadı").arg(report.failed)
                               : QStringLiteral("%1 tweak uygulanamadı · %2")
                                     .arg(report.failed).arg(report.firstError));
}

void MainWindow::onRevert()
{
    m_state->revertPending();
    refreshView();
}

void MainWindow::onRestorePointRequested()
{
    // Creating a restore point changes the system, which this build deliberately does
    // not do — so hand the user Windows' own System Protection dialog instead.
#ifdef Q_OS_WIN
    if (QProcess::startDetached(QStringLiteral("SystemPropertiesProtection.exe"), {}))
        return;
#endif
    QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:about")));
}
