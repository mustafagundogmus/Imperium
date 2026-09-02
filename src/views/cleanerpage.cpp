#include "cleanerpage.h"

#include "../cleaner.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/buttons.h"
#include "../widgets/cleanerrow.h"
#include "../widgets/dialog.h"
#include "../widgets/sectionheader.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

namespace {

constexpr int TopBarGap = 10;

QString formatBytes(qint64 bytes)
{
    return QLocale().formattedDataSize(qMax<qint64>(0, bytes), 1, QLocale::DataSizeTraditionalFormat);
}

constexpr Cleaner::Group GroupOrder[] = {Cleaner::Group::System, Cleaner::Group::User,
                                         Cleaner::Group::Apps, Cleaner::Group::Advanced};

} // namespace

CleanerPage::CleanerPage(QWidget *parent)
    : QWidget(parent)
    , m_engine(new Cleaner::Engine(this))
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
    m_summaryLabel->setFont(Theme::Font::pageSub());
    QPalette pal = m_summaryLabel->palette();
    pal.setColor(QPalette::WindowText, Theme::Color::TextFaint());
    m_summaryLabel->setPalette(pal);
    topLayout->addWidget(m_summaryLabel);
    topLayout->addStretch(1);

    m_rescanButton = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("cleaner.rescan")), topBar);
    connect(m_rescanButton, &PillButton::clicked, this, [this] { rescan(); });
    topLayout->addWidget(m_rescanButton);

    m_selectAllButton = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("cleaner.selectAll")), topBar);
    connect(m_selectAllButton, &PillButton::clicked, this, &CleanerPage::toggleSelectAll);
    topLayout->addWidget(m_selectAllButton);

    m_cleanButton = new PillButton(PillButton::Accent,
                                   Locale::tr(QStringLiteral("cleaner.cleanSelected")).arg(formatBytes(0)),
                                   topBar);
    connect(m_cleanButton, &PillButton::clicked, this, &CleanerPage::cleanSelected);
    topLayout->addWidget(m_cleanButton);

    m_layout->addWidget(topBar);
    m_layout->addStretch(1);

    for (const Cleaner::Target &t : Cleaner::targets())
        m_checked.insert(t.id, t.defaultOn);

    connect(m_engine, &Cleaner::Engine::measured, this,
            [this](const QString &id, qint64 bytes, qint64 files) {
                m_bytes.insert(id, bytes);
                m_files.insert(id, files);
                if (CleanerRow *row = m_rows.value(id))
                    row->setSize(sizeText(id));
                updateBar();
            });
    connect(m_engine, &Cleaner::Engine::scanFinished, this, [this] {
        m_rescanButton->setEnabledLook(true);
        updateBar();
        Q_EMIT scanFinished();
    });
    connect(m_engine, &Cleaner::Engine::cleaned, this,
            [this](const QString &id, qint64 freed, qint64 skipped, const QString &error) {
                const Cleaner::Target *t = Cleaner::target(id);
                QString line;
                if (!error.isEmpty())
                    line = Locale::tr(QStringLiteral("cleaner.row.failed")).arg(error);
                else if (t && t->kind == Cleaner::Kind::ComponentStore)
                    line = Locale::tr(QStringLiteral("cleaner.row.done"));
                else
                    line = Locale::tr(QStringLiteral("cleaner.row.freed")).arg(formatBytes(freed));
                if (skipped > 0 && error.isEmpty())
                    line += QStringLiteral(" · ")
                            + Locale::tr(QStringLiteral("cleaner.row.skipped")).arg(skipped);
                m_results.insert(id, line);
                if (CleanerRow *row = m_rows.value(id)) {
                    row->setBusy(false);
                    row->setStatus(line);
                }
            });
    connect(m_engine, &Cleaner::Engine::cleanFinished, this, [this](qint64 freed) {
        m_pendingIds.clear();
        Q_EMIT notice(Locale::tr(QStringLiteral("cleaner.notice.result")).arg(formatBytes(freed)));
        // The machine changed; measure it again rather than subtract. The result lines
        // stay on the rows until the new figures arrive.
        rescan();
    });

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &CleanerPage::retranslate);

    rebuild();
    rescan();
}

bool CleanerPage::scanning() const
{
    return m_engine->scanning();
}

qint64 CleanerPage::reclaimableBytes() const
{
    qint64 total = 0;
    for (auto it = m_bytes.cbegin(); it != m_bytes.cend(); ++it)
        if (it.value() > 0)
            total += it.value();
    return total;
}

QString CleanerPage::reclaimableText() const
{
    return formatBytes(reclaimableBytes());
}

void CleanerPage::rescan()
{
    if (m_engine->busy())
        return;
    m_bytes.clear();
    m_files.clear();
    for (CleanerRow *row : std::as_const(m_rows))
        row->setSize(QStringLiteral("…"));
    m_summaryLabel->setText(Locale::tr(QStringLiteral("cleaner.scanning")));
    m_rescanButton->setEnabledLook(false);
    m_engine->scan();
}

QString CleanerPage::sizeText(const QString &id) const
{
    if (!m_bytes.contains(id))
        return QStringLiteral("…");
    const qint64 bytes = m_bytes.value(id);
    if (bytes < 0)
        return QStringLiteral("—");
    return formatBytes(bytes);
}

QString CleanerPage::descriptionFor(const QString &id) const
{
    const Cleaner::Target *t = Cleaner::target(id);
    if (!t)
        return {};
    QString desc = Locale::tr(t->descKey());
    if (!t->noteKey.isEmpty())
        desc += QStringLiteral(" · ") + Locale::tr(QStringLiteral("svc.riskPrefix"))
                + Locale::tr(t->noteKey);
    return desc;
}

void CleanerPage::rebuild()
{
    if (m_body) {
        m_layout->removeWidget(m_body);
        m_body->hide();
        m_body->deleteLater();
        m_body = nullptr;
    }
    m_rows.clear();
    m_headers.clear();
    m_rowCount = 0;

    m_body = new QWidget(this);
    auto *body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(Theme::Metric::SectionGap);

    for (Cleaner::Group group : GroupOrder) {
        QVector<const Cleaner::Target *> here;
        for (const Cleaner::Target &t : Cleaner::targets())
            if (t.group == group)
                here.append(&t);
        if (here.isEmpty())
            continue;

        auto *header = new SectionHeader(Locale::tr(Cleaner::groupKey(group)), m_body);
        body->addWidget(header);
        m_headers.append(header);

        auto *list = new QWidget(m_body);
        auto *listLayout = new QVBoxLayout(list);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(1);

        for (const Cleaner::Target *t : std::as_const(here)) {
            auto *row = new CleanerRow(t->id, Locale::tr(t->nameKey()), descriptionFor(t->id), list);
            row->setChecked(m_checked.value(t->id, t->defaultOn));
            row->setSize(sizeText(t->id));
            if (m_results.contains(t->id))
                row->setStatus(m_results.value(t->id));
            if (m_pendingIds.contains(t->id))
                row->setBusy(true);
            connect(row, &CleanerRow::toggled, this, [this](const QString &id, bool on) {
                m_checked.insert(id, on);
                updateBar();
            });
            listLayout->addWidget(row);
            m_rows.insert(t->id, row);
            ++m_rowCount;
        }
        body->addWidget(list);
    }

    m_layout->insertWidget(1, m_body);
    updateBar();
}

void CleanerPage::updateBar()
{
    qint64 selected = 0;
    int checked = 0;
    const int total = int(Cleaner::targets().size());
    for (const Cleaner::Target &t : Cleaner::targets()) {
        if (!m_checked.value(t.id))
            continue;
        ++checked;
        selected += qMax<qint64>(0, m_bytes.value(t.id, 0));
    }

    // Section counts: what each group would free.
    int index = 0;
    for (Cleaner::Group group : GroupOrder) {
        qint64 sum = 0;
        bool any = false;
        for (const Cleaner::Target &t : Cleaner::targets()) {
            if (t.group != group)
                continue;
            any = true;
            sum += qMax<qint64>(0, m_bytes.value(t.id, 0));
        }
        if (!any)
            continue;
        if (index < m_headers.size())
            m_headers.at(index)->setCount(formatBytes(sum));
        ++index;
    }

    const bool busy = m_engine->busy();
    if (!m_engine->scanning())
        m_summaryLabel->setText(Locale::tr(QStringLiteral("cleaner.subtitle"))
                                    .arg(total)
                                    .arg(reclaimableText()));
    m_cleanButton->setText(Locale::tr(QStringLiteral("cleaner.cleanSelected")).arg(formatBytes(selected)));
    m_cleanButton->setEnabledLook(checked > 0 && !busy);
    m_selectAllButton->setEnabledLook(!busy);
    m_selectAllButton->setText(Locale::tr(checked == total ? QStringLiteral("cleaner.clearSelection")
                                                           : QStringLiteral("cleaner.selectAll")));
    m_rescanButton->setEnabledLook(!busy);
}

void CleanerPage::toggleSelectAll()
{
    int checked = 0;
    for (const Cleaner::Target &t : Cleaner::targets())
        if (m_checked.value(t.id))
            ++checked;
    const bool selectAll = checked < Cleaner::targets().size();
    for (const Cleaner::Target &t : Cleaner::targets()) {
        m_checked.insert(t.id, selectAll);
        if (CleanerRow *row = m_rows.value(t.id))
            row->setChecked(selectAll);
    }
    updateBar();
}

void CleanerPage::cleanSelected()
{
    if (m_engine->busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("cleaner.notice.busy")));
        return;
    }

    QStringList ids;
    QStringList lines;
    qint64 bytes = 0;
    for (const Cleaner::Target &t : Cleaner::targets()) {
        if (!m_checked.value(t.id))
            continue;
        ids << t.id;
        bytes += qMax<qint64>(0, m_bytes.value(t.id, 0));
        lines << Locale::tr(t.nameKey()) + QStringLiteral("  —  ") + sizeText(t.id);
    }
    if (ids.isEmpty())
        return;

    const bool go = Dialog::confirm(
        this, Locale::tr(QStringLiteral("cleaner.title")),
        Locale::tr(QStringLiteral("cleaner.confirm.body")).arg(ids.size()).arg(formatBytes(bytes))
            + Locale::tr(QStringLiteral("actions.confirm.irreversible")) + QStringLiteral("\n")
            + Locale::tr(QStringLiteral("cleaner.confirm.note")),
        Locale::tr(QStringLiteral("cleaner.confirm.accept")),
        Locale::tr(QStringLiteral("actions.cancel")),
        lines.join(QLatin1Char('\n')));
    if (!go)
        return;

    m_pendingIds = ids;
    m_results.clear();
    for (const QString &id : ids) {
        if (CleanerRow *row = m_rows.value(id)) {
            row->setBusy(true);
            row->setStatus(Locale::tr(QStringLiteral("cleaner.row.cleaning")));
        }
    }
    updateBar();
    m_engine->clean(ids);
}

void CleanerPage::retranslate()
{
    m_rescanButton->setText(Locale::tr(QStringLiteral("cleaner.rescan")));
    // Result lines are sentences in the old language; a rebuild puts the descriptions
    // back, which is the honest answer until the next clean.
    m_results.clear();
    rebuild();
    if (m_engine->scanning())
        m_summaryLabel->setText(Locale::tr(QStringLiteral("cleaner.scanning")));
}
