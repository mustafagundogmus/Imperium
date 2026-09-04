#include "featurespage.h"

#include "../fluent/fluenticons.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/buttons.h"
#include "../widgets/dialog.h"
#include "../widgets/featurerow.h"
#include "../widgets/sectionheader.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace {

constexpr int TopBarGap = 10;
constexpr int LogMaxHeight = 220;

void tint(QLabel *label, const QFont &font, const QColor &colour)
{
    label->setFont(font);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, colour);
    label->setPalette(pal);
}

QString logStyle()
{
    using namespace Theme;
    const QFont &f = Font::infoValueMono();
    return QStringLiteral(
               "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; "
               "border-radius: %4px; padding: 6px; font-family: \"%5\"; font-size: %6px; "
               "selection-background-color: %7; }")
        .arg(Color::Tile().name(), Color::TextPrimary().name(), Color::TileBorder().name())
        .arg(Metric::ControlRadius)
        .arg(f.family())
        .arg(pixelSize(f))
        .arg(Theme::accentSoft().name(QColor::HexArgb));
}

/// The glyph a row wears, by what it is about.
const Icons::Glyph *glyphFor(const QString &slug)
{
    using namespace FluentIcons::Lucide;
    using namespace Icons::Lucide;
    if (slug == QLatin1String("dotnet"))       return &Layers;
    if (slug == QLatin1String("hyperv"))       return &Box;
    if (slug == QLatin1String("legacymedia"))  return &Volume2;
    if (slug == QLatin1String("wsl"))          return &AppWindow;
    if (slug == QLatin1String("nfs"))          return &Folder;
    if (slug == QLatin1String("regbackup"))    return &HardDrive;
    if (slug == QLatin1String("sandbox"))      return &LayoutGrid;
    if (slug.contains(QLatin1String("legacyrecovery"))) return &Power;
    return &Blocks;
}

} // namespace

FeaturesPage::FeaturesPage(QWidget *parent)
    : QWidget(parent)
    , m_runner(new Features::Runner(this))
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                                 Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    m_layout->setSpacing(Theme::Metric::SectionGap);

    auto *topBar = new QWidget(this);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(TopBarGap);

    m_summaryLabel = new QLabel(topBar);
    tint(m_summaryLabel, Theme::Font::pageSub(), Theme::Color::TextFaint());
    topLayout->addWidget(m_summaryLabel);
    topLayout->addStretch(1);

    m_logButton = new PillButton(PillButton::Ghost, QString(), topBar);
    connect(m_logButton, &PillButton::clicked, this, [this] {
        m_log->setVisible(!m_log->isVisible());
        m_logButton->setText(Locale::tr(m_log->isVisible() ? QStringLiteral("apps.log.hide")
                                                           : QStringLiteral("apps.log.show")));
    });
    topLayout->addWidget(m_logButton);

    m_rescanButton = new PillButton(PillButton::Ghost, QString(), topBar);
    connect(m_rescanButton, &PillButton::clicked, this, &FeaturesPage::rescan);
    topLayout->addWidget(m_rescanButton);

    m_selectAllButton = new PillButton(PillButton::Ghost, QString(), topBar);
    connect(m_selectAllButton, &PillButton::clicked, this, &FeaturesPage::toggleSelectAll);
    topLayout->addWidget(m_selectAllButton);

    m_installButton = new PillButton(PillButton::Accent, QString(), topBar);
    connect(m_installButton, &PillButton::clicked, this, &FeaturesPage::installSelected);
    topLayout->addWidget(m_installButton);

    m_layout->addWidget(topBar);

    m_progressLabel = new QLabel(this);
    tint(m_progressLabel, Theme::Font::tweakDesc(), Theme::Color::TextSecondary());
    m_progressLabel->hide();
    m_layout->addWidget(m_progressLabel);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(4000);
    m_log->setMaximumHeight(LogMaxHeight);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_log->setStyleSheet(logStyle());
    m_log->hide();
    m_layout->addWidget(m_log);

    m_layout->addStretch(1);

    connect(m_runner, &Features::Runner::started, this, &FeaturesPage::onStarted);
    connect(m_runner, &Features::Runner::progress, this, &FeaturesPage::onProgress);
    connect(m_runner, &Features::Runner::line, this, &FeaturesPage::onLine);
    connect(m_runner, &Features::Runner::finished, this, &FeaturesPage::onFinished);
    connect(m_runner, &Features::Runner::scanned, this, &FeaturesPage::onScanned);

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &FeaturesPage::retranslate);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this,
            [this] { m_log->setStyleSheet(logStyle()); });
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this,
            [this] { m_log->setStyleSheet(logStyle()); });

    retranslate();
    rebuild();
    rescan();
}

int FeaturesPage::rowCount() const
{
    return Features::Catalogue::instance().count();
}

int FeaturesPage::enabledCount() const
{
    int n = 0;
    for (const Features::Entry &e : Features::Catalogue::instance().entries())
        if (m_machine.stateOf(e) == Features::State::Enabled)
            ++n;
    return n;
}

bool FeaturesPage::scanning() const
{
    return m_scanning;
}

bool FeaturesPage::busy() const
{
    return m_runner->running() && m_runner->job() != Features::Runner::Job::Scan;
}

QString FeaturesPage::subtitle() const
{
    if (busy() && !m_progressText.isEmpty())
        return m_progressText;
    if (m_scanning)
        return Locale::tr(QStringLiteral("features.scanning"));
    return Locale::tr(QStringLiteral("features.subtitle")).arg(rowCount()).arg(enabledCount());
}

void FeaturesPage::rescan()
{
    if (m_runner->running())
        return;
    m_scanning = true;
    m_summaryLabel->setText(Locale::tr(QStringLiteral("features.scanning")));
    m_rescanButton->setEnabledLook(false);
    m_runner->scan();
    Q_EMIT stateChanged();
}

void FeaturesPage::rebuild()
{
    if (m_body) {
        m_layout->removeWidget(m_body);
        m_body->hide();
        m_body->deleteLater();
        m_body = nullptr;
    }
    m_rows.clear();
    m_headers.clear();

    m_body = new QWidget(this);
    auto *body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(Theme::Metric::SectionGap);

    const Features::Catalogue &catalogue = Features::Catalogue::instance();
    struct Group { const char *key; bool dism; };
    static const Group groups[] = {
        {"features.section.dism", true},
        {"features.section.scripts", false},
    };

    for (const Group &group : groups) {
        QVector<const Features::Entry *> here;
        for (const Features::Entry &e : catalogue.entries())
            if (e.isDism() == group.dism)
                here.append(&e);
        if (here.isEmpty())
            continue;

        auto *header = new SectionHeader(Locale::tr(QString::fromLatin1(group.key)), m_body);
        header->setCount(Locale::tr(QStringLiteral("features.count")).arg(here.size()));
        body->addWidget(header);
        m_headers.append(header);

        auto *list = new QWidget(m_body);
        auto *listLayout = new QVBoxLayout(list);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(1);

        for (const Features::Entry *e : std::as_const(here)) {
            auto *row = new FeatureRow(e->key, glyphFor(e->slug), list);
            row->setName(Locale::tr(e->nameKey()));

            // The technical identity under the friendly name: the DISM feature names are
            // what Enable-WindowsOptionalFeature is actually handed.
            QString desc = Locale::tr(e->descKey());
            if (e->isDism())
                desc += QStringLiteral(" · ") + e->features.join(QStringLiteral(", "));
            row->setDesc(desc);

            const Features::State state = m_machine.stateOf(*e);
            QString word;
            FeatureRow::Tone tone = FeatureRow::Tone::Muted;
            switch (state) {
            case Features::State::Enabled:
                word = Locale::tr(QStringLiteral("features.state.enabled"));
                tone = FeatureRow::Tone::On;
                break;
            case Features::State::Disabled:
                word = Locale::tr(QStringLiteral("features.state.disabled"));
                break;
            case Features::State::Partial:
                word = Locale::tr(QStringLiteral("features.state.partial"));
                tone = FeatureRow::Tone::Warn;
                break;
            case Features::State::Unavailable:
                word = Locale::tr(QStringLiteral("features.state.unavailable"));
                break;
            case Features::State::Unknown:
                word = m_scanning ? QString() : Locale::tr(QStringLiteral("features.state.unknown"));
                break;
            }
            row->setState(word, tone);
            row->setSelectable(state != Features::State::Unavailable);
            row->setChecked(m_checked.contains(e->key));
            if (m_results.contains(e->key))
                row->setStatus(m_results.value(e->key));

            // Turning a DISM feature off: the one thing here WinUtil has no button for.
            const bool canDisable = e->isDism()
                                    && (state == Features::State::Enabled
                                        || state == Features::State::Partial);
            row->actionButton()->setText(Locale::tr(QStringLiteral("features.disable")));
            row->setActionVisible(canDisable);
            if (canDisable) {
                const QString key = e->key;
                connect(row->actionButton(), &PillButton::clicked, this, [this, key] { disableOne(key); });
            }
            connect(row, &FeatureRow::toggled, this, [this](const QString &key, bool on) {
                if (on)
                    m_checked.insert(key);
                else
                    m_checked.remove(key);
                updateBar();
            });

            listLayout->addWidget(row);
            m_rows.insert(e->key, row);
        }
        body->addWidget(list);
    }

    m_layout->insertWidget(1, m_body);

    m_summaryLabel->setText(m_scanning ? Locale::tr(QStringLiteral("features.scanning"))
                                       : Locale::tr(QStringLiteral("features.summary"))
                                             .arg(rowCount())
                                             .arg(enabledCount()));
    const bool running = m_runner->running();
    if (running)
        for (const QString &key : std::as_const(m_pendingKeys))
            if (FeatureRow *row = m_rows.value(key))
                row->setBusy(true);
    m_rescanButton->setEnabledLook(!running);
    updateBar();
}

void FeaturesPage::retranslate()
{
    m_rescanButton->setText(Locale::tr(QStringLiteral("features.rescan")));
    m_logButton->setText(Locale::tr(m_log->isVisible() ? QStringLiteral("apps.log.hide")
                                                       : QStringLiteral("apps.log.show")));
    if (m_body)
        rebuild();
    else
        updateBar();
}

void FeaturesPage::updateBar()
{
    int checked = 0;
    int selectable = 0;
    for (FeatureRow *row : std::as_const(m_rows)) {
        if (!row->selectable())
            continue;
        ++selectable;
        if (row->checked())
            ++checked;
    }
    const bool idle = !busy();
    m_installButton->setText(Locale::tr(QStringLiteral("features.install")).arg(checked));
    m_installButton->setEnabledLook(checked > 0 && idle);
    m_selectAllButton->setEnabledLook(selectable > 0 && idle);
    m_selectAllButton->setText(Locale::tr(checked > 0 && checked == selectable
                                              ? QStringLiteral("features.clearSelection")
                                              : QStringLiteral("features.selectAll")));
    Q_EMIT stateChanged();
}

void FeaturesPage::toggleSelectAll()
{
    int checked = 0;
    int selectable = 0;
    for (FeatureRow *row : std::as_const(m_rows)) {
        if (!row->selectable())
            continue;
        ++selectable;
        if (row->checked())
            ++checked;
    }
    const bool selectAll = checked < selectable;
    for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it) {
        if (!it.value()->selectable())
            continue;
        it.value()->setChecked(selectAll);
        if (selectAll)
            m_checked.insert(it.key());
        else
            m_checked.remove(it.key());
    }
    updateBar();
}

QVector<Features::Entry> FeaturesPage::checkedEntries() const
{
    QVector<Features::Entry> out;
    for (const Features::Entry &e : Features::Catalogue::instance().entries())
        if (m_checked.contains(e.key))
            if (FeatureRow *row = m_rows.value(e.key); row && row->checked())
                out.append(e);
    return out;
}

void FeaturesPage::installSelected()
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    const QVector<Features::Entry> entries = checkedEntries();
    if (entries.isEmpty()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("features.notice.selectFirst")));
        return;
    }

    QStringList names;
    for (const Features::Entry &e : entries)
        names << Locale::tr(e.nameKey());

    const bool go = Dialog::confirm(
        this, Locale::tr(QStringLiteral("features.confirm.title")).arg(entries.size()),
        Locale::tr(QStringLiteral("features.confirm.body")) + QStringLiteral("\n\n")
            + names.join(QStringLiteral(" · ")),
        Locale::tr(QStringLiteral("features.confirm.run")),
        Locale::tr(QStringLiteral("actions.cancel")),
        Features::installSummary(entries));
    if (!go)
        return;

    m_pendingKeys.clear();
    for (const Features::Entry &e : entries) {
        m_pendingKeys << e.key;
        m_results.remove(e.key);
        if (FeatureRow *row = m_rows.value(e.key))
            row->setStatus(QString());
    }
    m_runner->install(entries);
}

void FeaturesPage::disableOne(const QString &key)
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    const Features::Entry *e = Features::Catalogue::instance().entry(key);
    if (!e || !e->isDism())
        return;

    const bool go = Dialog::confirm(
        this, Locale::tr(QStringLiteral("features.confirm.disable.title")).arg(Locale::tr(e->nameKey())),
        Locale::tr(QStringLiteral("features.confirm.disable.body")),
        Locale::tr(QStringLiteral("features.disable")),
        Locale::tr(QStringLiteral("actions.cancel")),
        Features::disableSummary(e->features));
    if (!go)
        return;

    m_pendingKeys = {key};
    m_results.remove(key);
    if (FeatureRow *row = m_rows.value(key))
        row->setStatus(QString());
    m_runner->disable(e->features);
}

void FeaturesPage::onStarted(Features::Runner::Job job, int total)
{
    using Job = Features::Runner::Job;
    if (job == Job::Scan)
        return;
    m_log->clear();
    for (const QString &key : std::as_const(m_pendingKeys))
        if (FeatureRow *row = m_rows.value(key))
            row->setBusy(true);
    m_progressText = Locale::tr(job == Job::Install ? QStringLiteral("features.progress.preparing")
                                                    : QStringLiteral("features.progress.disabling"))
                         .arg(total);
    m_progressLabel->setText(m_progressText);
    m_progressLabel->show();
    m_rescanButton->setEnabledLook(false);
    updateBar();
}

void FeaturesPage::onProgress(int done, int total, const QString &key, bool finished, bool ok)
{
    using Job = Features::Runner::Job;
    const bool install = m_runner->job() == Job::Install;

    // A disable reports DISM feature names, an install reports row keys.
    QString name = key;
    QString rowKey;
    if (const Features::Entry *e = Features::Catalogue::instance().entry(key)) {
        name = Locale::tr(e->nameKey());
        rowKey = key;
    } else {
        for (const QString &pending : std::as_const(m_pendingKeys))
            if (const Features::Entry *p = Features::Catalogue::instance().entry(pending))
                if (p->features.contains(key))
                    rowKey = pending;
    }

    const int shown = finished ? done : done + 1;
    m_progressText = Locale::tr(install ? (finished ? QStringLiteral("features.progress.done")
                                                    : QStringLiteral("features.progress.running"))
                                        : (finished ? QStringLiteral("features.progress.disabled")
                                                    : QStringLiteral("features.progress.disablingOne")))
                         .arg(name).arg(shown).arg(total);
    m_progressLabel->setText(m_progressText);

    if (finished && !rowKey.isEmpty()) {
        const QString word = ok ? Locale::tr(install ? QStringLiteral("features.result.installed")
                                                     : QStringLiteral("features.result.disabled"))
                                : Locale::tr(QStringLiteral("features.result.failed"));
        if (!ok || install)
            m_results.insert(rowKey, word);
        if (FeatureRow *row = m_rows.value(rowKey))
            row->setStatus(word);
    }
    Q_EMIT stateChanged();
}

void FeaturesPage::onLine(const QString &text)
{
    m_log->appendPlainText(text);
}

void FeaturesPage::onFinished(Features::Runner::Job job, bool ok, int failures)
{
    using Job = Features::Runner::Job;
    if (job == Job::Scan) {
        // onScanned did the work; the rescan button comes back in rebuild().
        return;
    }

    for (const QString &key : std::as_const(m_pendingKeys))
        if (FeatureRow *row = m_rows.value(key))
            row->setBusy(false);

    if (job == Job::Install) {
        if (ok && failures == 0)
            Q_EMIT notice(Locale::tr(QStringLiteral("features.notice.installed")));
        else if (failures > 0)
            Q_EMIT notice(Locale::tr(QStringLiteral("features.notice.failures")).arg(failures));
        else
            Q_EMIT notice(Locale::tr(QStringLiteral("features.notice.aborted")));
    } else if (job == Job::Disable) {
        if (ok && failures == 0)
            Q_EMIT notice(Locale::tr(QStringLiteral("features.notice.disabled")));
        else
            Q_EMIT notice(Locale::tr(QStringLiteral("features.notice.disableFailed")));
    }

    m_pendingKeys.clear();
    m_progressText.clear();
    m_progressLabel->hide();
    updateBar();
    rescan();   // the machine changed, or tried to; DISM is the one to ask
}

void FeaturesPage::onScanned(const Features::Machine &machine)
{
    m_scanning = false;
    m_machine = machine;
    if (!machine.valid)
        Q_EMIT notice(Locale::tr(QStringLiteral("features.notice.scanFailed")));
    rebuild();
    Q_EMIT scanFinished();
}
