#include "appspage.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"
#include "../widgets/appentry.h"
#include "../widgets/buttons.h"
#include "../widgets/dialog.h"
#include "../widgets/flowlayout.h"
#include "../widgets/searchfield.h"
#include "../widgets/segmentedcontrol.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

constexpr int BarGap = 8;
constexpr int TileGapX = 6;
constexpr int TileGapY = 6;
constexpr int BlockGap = 14;
constexpr int SearchDebounceMs = 300;   // WinUtil's SearchBarTimer
constexpr int LogMaxHeight = 220;
constexpr int PopupMaxRows = 12;

const QString KeyManager = QStringLiteral("apps/packageManager");

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

} // namespace

// ------------------------------------------------------------------- Chip ---

/// A category filter chip: WinUtil's FilterChipToggleStyle. Ctrl+click adds to the
/// selection instead of replacing it, which the click signal carries.
class AppsPage::Chip : public QWidget
{
public:
    Chip(const QString &category, QWidget *parent)
        : QWidget(parent)
        , m_category(category)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
        connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
        connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
            setFixedSize(sizeHint());
            update();
        });
    }

    QString category() const { return m_category; }

    void setText(const QString &text)
    {
        m_text = text;
        setFixedSize(sizeHint());
        update();
    }

    void setChecked(bool on)
    {
        if (m_checked == on)
            return;
        m_checked = on;
        update();
    }

    QSize sizeHint() const override
    {
        const QFont &f = Theme::Font::segment();
        return {qRound(Css::textWidth(f, m_text) + 2 * PadX),
                qRound(Css::normalLine(f) + 2 * PadY)};
    }

    std::function<void(const QString &, bool)> onClick;

protected:
    void paintEvent(QPaintEvent *) override
    {
        using namespace Theme;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = r.height() / 2.0;
        p.setPen(QPen(m_checked ? Theme::accent() : Color::BorderControl(), 1.0));
        p.setBrush(m_checked ? Theme::accentSoft() : (m_hovered ? Color::SurfaceHover() : Color::Surface()));
        p.drawRoundedRect(r, radius, radius);
        const QColor ink = m_checked ? Color::TextPrimary()
                                     : (m_hovered ? Color::TextSecondary() : Color::TextMuted());
        Css::drawCentered(&p, r, Font::segment(), ink, m_text, Qt::AlignHCenter);
    }
    void enterEvent(QEnterEvent *e) override { m_hovered = true; update(); QWidget::enterEvent(e); }
    void leaveEvent(QEvent *e) override { m_hovered = false; update(); QWidget::leaveEvent(e); }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) { e->accept(); return; }
        QWidget::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            e->accept();
            if (rect().contains(e->pos()) && onClick)
                onClick(m_category, e->modifiers().testFlag(Qt::ControlModifier));
            return;
        }
        QWidget::mouseReleaseEvent(e);
    }

private:
    static constexpr qreal PadX = 10.0;
    static constexpr qreal PadY = 3.0;

    QString m_category;
    QString m_text;
    bool m_checked = false;
    bool m_hovered = false;
};

// --------------------------------------------------------- CategoryHeader ---

/// WinUtil's "- Category" / "+ Category" label: a section heading that folds its panel.
class AppsPage::CategoryHeader : public QWidget
{
public:
    explicit CategoryHeader(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setFixedHeight(sizeHint().height());
        connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
        connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
            setFixedHeight(sizeHint().height());
            updateGeometry();
            update();
        });
    }

    void setTitle(const QString &title) { m_title = Css::upperTr(title); update(); }
    void setCount(const QString &count) { m_count = count; update(); }
    void setCollapsed(bool on) { m_collapsed = on; update(); }
    bool collapsed() const { return m_collapsed; }

    std::function<void()> onClick;

    QSize sizeHint() const override
    {
        return {0, qRound(Css::normalLine(Theme::Font::sectionTitle()) + 8.0)};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        using namespace Theme;
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const QFont &labelFont = Font::sectionTitle();
        const qreal line = Css::normalLine(labelFont);
        const QRectF content(PadX, 0, width() - 2 * PadX, line);
        const QString text = (m_collapsed ? QStringLiteral("+ ") : QStringLiteral("− ")) + m_title;
        const QColor ink = m_hovered ? Color::TextSecondary() : Color::TextDim();
        Css::drawText(&p, content, Css::baseline(labelFont, 0, line), labelFont, ink, text);
        const qreal labelW = Css::textWidth(labelFont, text);

        qreal countW = 0;
        if (!m_count.isEmpty()) {
            const QFont &countFont = Font::sectionCount();
            countW = Css::textWidth(countFont, m_count);
            Css::drawText(&p, content, Css::centeredBaseline(countFont, content), countFont,
                          Color::TextFainter(), m_count, Qt::AlignRight);
        }
        const qreal ruleLeft = content.left() + labelW + Gap;
        const qreal ruleRight = content.right() - (countW > 0 ? countW + Gap : 0.0);
        if (ruleRight > ruleLeft)
            Css::hairline(&p, QRectF(ruleLeft, std::round((line - 1.0) / 2.0), ruleRight - ruleLeft, 1.0),
                          Color::Divider());
    }
    void enterEvent(QEnterEvent *e) override { m_hovered = true; update(); QWidget::enterEvent(e); }
    void leaveEvent(QEvent *e) override { m_hovered = false; update(); QWidget::leaveEvent(e); }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) { e->accept(); return; }
        QWidget::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            e->accept();
            if (rect().contains(e->pos()) && onClick)
                onClick();
            return;
        }
        QWidget::mouseReleaseEvent(e);
    }

private:
    static constexpr qreal PadX = 6.0;
    static constexpr qreal Gap = 10.0;

    QString m_title;
    QString m_count;
    bool m_collapsed = false;
    bool m_hovered = false;
};

// ---------------------------------------------------------- ProgressStrip ---

/// WinUtil's tweaks progress indicator: a sentence, a percentage and a thin rail.
class AppsPage::ProgressStrip : public QWidget
{
public:
    explicit ProgressStrip(QWidget *parent)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setFixedHeight(sizeHint().height());
        connect(Theme::notifier(), &Theme::Notifier::accentChanged, this, qOverload<>(&QWidget::update));
        connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
        connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this, [this] {
            setFixedHeight(sizeHint().height());
            updateGeometry();
            update();
        });
    }

    void set(const QString &text, int percent, bool failed)
    {
        m_text = text;
        m_percent = qBound(0, percent, 100);
        m_failed = failed;
        update();
    }

    QSize sizeHint() const override
    {
        return {0, qRound(Css::normalLine(Theme::Font::tweakDesc()) + RailGap + RailHeight + PadY * 2)};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        using namespace Theme;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const QFont &f = Font::tweakDesc();
        const qreal line = Css::normalLine(f);
        const QRectF textBox(PadX, PadY, width() - 2 * PadX, line);
        const QString pct = QStringLiteral("%1%").arg(m_percent);
        const qreal pctW = Css::textWidth(Font::sectionCount(), pct);
        Css::drawText(&p, textBox.adjusted(0, 0, -(pctW + 8), 0), Css::baseline(f, PadY, line), f,
                      m_failed ? Color::Danger() : Color::TextSecondary(), m_text, Qt::AlignLeft, true);
        Css::drawText(&p, textBox, Css::centeredBaseline(Font::sectionCount(), textBox),
                      Font::sectionCount(), Color::TextFaint(), pct, Qt::AlignRight);

        const QRectF rail(PadX, PadY + line + RailGap, width() - 2 * PadX, RailHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(Color::Surface());
        p.drawRoundedRect(rail, RailHeight / 2.0, RailHeight / 2.0);
        if (m_percent > 0) {
            QRectF fill = rail;
            fill.setWidth(rail.width() * m_percent / 100.0);
            p.setBrush(m_failed ? Color::Danger() : Theme::accent());
            p.drawRoundedRect(fill, RailHeight / 2.0, RailHeight / 2.0);
        }
    }

private:
    static constexpr qreal PadX = 6.0;
    static constexpr qreal PadY = 4.0;
    static constexpr qreal RailGap = 4.0;
    static constexpr qreal RailHeight = 3.0;

    QString m_text;
    int m_percent = 0;
    bool m_failed = false;
};

// --------------------------------------------------------- SelectionPopup ---

/// The "Selected Apps: N" popup — every selected name, alphabetical, each with a remove
/// button that unticks the tile (Add-SelectedAppsMenuItem).
class AppsPage::SelectionPopup : public QWidget
{
public:
    explicit SelectionPopup(QWidget *parent)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground, false);
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(1, 1, 1, 1);
        m_scroll = new QScrollArea(this);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setWidgetResizable(true);
        m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scroll->viewport()->setAutoFillBackground(false);
        m_list = new QWidget(m_scroll);
        m_list->setAutoFillBackground(false);
        m_listLayout = new QVBoxLayout(m_list);
        m_listLayout->setContentsMargins(6, 6, 6, 6);
        m_listLayout->setSpacing(2);
        m_scroll->setWidget(m_list);
        outer->addWidget(m_scroll);
    }

    /// \a rows are (key, name) pairs, already sorted.
    void rebuild(const QVector<QPair<QString, QString>> &rows, const QString &emptyText,
                 const QString &removeText, std::function<void(const QString &)> onRemove)
    {
        while (QLayoutItem *item = m_listLayout->takeAt(0)) {
            if (QWidget *w = item->widget())
                w->deleteLater();
            delete item;
        }
        if (rows.isEmpty()) {
            auto *label = new QLabel(emptyText, m_list);
            tint(label, Theme::Font::tweakDesc(), Theme::Color::TextFaint());
            m_listLayout->addWidget(label);
        }
        for (const auto &row : rows) {
            auto *line = new QWidget(m_list);
            auto *h = new QHBoxLayout(line);
            h->setContentsMargins(4, 0, 0, 0);
            h->setSpacing(8);
            auto *label = new QLabel(row.second, line);
            label->setToolTip(row.second);
            tint(label, Theme::Font::tweakName(), Theme::Color::TextPrimary());
            h->addWidget(label, 1);
            auto *remove = new PillButton(PillButton::Ghost, removeText, line);
            const QString key = row.first;
            QObject::connect(remove, &PillButton::clicked, line, [onRemove, key] { onRemove(key); });
            h->addWidget(remove);
            m_listLayout->addWidget(line);
        }
        m_listLayout->addStretch(1);

        const int rowH = qMax(PillButton(PillButton::Ghost, removeText).sizeHint().height(),
                              qRound(Css::normalLine(Theme::Font::tweakName())));
        const int shown = qBound(1, int(rows.size()), PopupMaxRows);
        const int width = qRound(Css::textWidth(Theme::Font::tweakName(), QStringLiteral("0")) * 32.0);
        resize(width, shown * (rowH + 2) + 14 + (rows.size() > PopupMaxRows ? 6 : 0));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Theme::Color::Tile());
        p.setPen(QPen(Theme::Color::TileBorder(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }

private:
    QScrollArea *m_scroll = nullptr;
    QWidget *m_list = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
};

// ------------------------------------------------------------- EntryPopup ---

/// The right-click popup on a tile: Install, Uninstall, Info — WinUtil's appPopup.
class AppsPage::EntryPopup : public QWidget
{
public:
    explicit EntryPopup(QWidget *parent)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    {
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(7, 7, 7, 7);
        h->setSpacing(6);
        m_install = new PillButton(PillButton::Accent, QString(), this);
        m_uninstall = new PillButton(PillButton::Ghost, QString(), this);
        m_info = new PillButton(PillButton::Ghost, QString(), this);
        h->addWidget(m_install);
        h->addWidget(m_uninstall);
        h->addWidget(m_info);
        QObject::connect(m_install, &PillButton::clicked, this, [this] { hide(); if (onInstall) onInstall(m_key); });
        QObject::connect(m_uninstall, &PillButton::clicked, this, [this] { hide(); if (onUninstall) onUninstall(m_key); });
        QObject::connect(m_info, &PillButton::clicked, this, [this] { hide(); if (onInfo) onInfo(m_key); });
    }

    void setTexts(const QString &install, const QString &uninstall, const QString &info)
    {
        m_install->setText(install);
        m_uninstall->setText(uninstall);
        m_info->setText(info);
        adjustSize();
    }

    void open(const QString &key, const QString &link, const QPoint &globalPos)
    {
        m_key = key;
        m_install->setToolTip(link);
        m_info->setToolTip(link);
        m_info->setEnabledLook(!link.isEmpty());
        adjustSize();
        move(globalPos);
        show();
    }

    std::function<void(const QString &)> onInstall, onUninstall, onInfo;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Theme::Color::Tile());
        p.setPen(QPen(Theme::Color::TileBorder(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }

private:
    QString m_key;
    PillButton *m_install = nullptr;
    PillButton *m_uninstall = nullptr;
    PillButton *m_info = nullptr;
};

// --------------------------------------------------------------- AppsPage ---

AppsPage::AppsPage(QWidget *parent)
    : QWidget(parent)
    , m_runner(new Apps::Runner(this))
{
    m_preference = Apps::managerFromString(
        QSettings().value(KeyManager, Apps::managerToString(Apps::Manager::Winget)).toString());

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                                 Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    m_layout->setSpacing(Theme::Metric::SectionGap);

    buildToolbar();
    buildFilters();
    buildList();
    m_layout->addStretch(1);

    m_selectionPopup = new SelectionPopup(this);
    m_entryPopup = new EntryPopup(this);
    m_entryPopup->onInstall = [this](const QString &key) { installOne(key); };
    m_entryPopup->onUninstall = [this](const QString &key) { uninstallOne(key); };
    m_entryPopup->onInfo = [this](const QString &key) { openLink(key); };

    connect(m_runner, &Apps::Runner::started, this, &AppsPage::onStarted);
    connect(m_runner, &Apps::Runner::progress, this, &AppsPage::onProgress);
    connect(m_runner, &Apps::Runner::line, this, &AppsPage::onLine);
    connect(m_runner, &Apps::Runner::finished, this, &AppsPage::onFinished);
    connect(m_runner, &Apps::Runner::detected, this, &AppsPage::onDetected);
    connect(m_runner, &Apps::Runner::probed, this, &AppsPage::onProbed);

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &AppsPage::retranslate);
    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this,
            [this] { m_log->setStyleSheet(logStyle()); });
    connect(Theme::notifier(), &Theme::Notifier::typefaceChanged, this,
            [this] { m_log->setStyleSheet(logStyle()); });

    retranslate();
    selectionChanged();

    // Test-WinUtilPackageManager, once, so the status line can say what is here.
    m_runner->probe();
}

int AppsPage::rowCount() const
{
    return Apps::Catalogue::instance().count();
}

int AppsPage::categoryCount() const
{
    return int(Apps::Catalogue::instance().categories().size());
}

bool AppsPage::busy() const
{
    return m_runner->running() && m_runner->job() != Apps::Runner::Job::Probe;
}

QString AppsPage::subtitle() const
{
    if (busy() && !m_progressText.isEmpty())
        return m_progressText;
    return Locale::tr(QStringLiteral("apps.subtitle"))
        .arg(rowCount())
        .arg(categoryCount())
        .arg(selectedCount());
}

// ------------------------------------------------------------- building ---

void AppsPage::buildToolbar()
{
    // Row one: the three actions and the package manager — appnavigation.json's
    // "Actions" and "Package Manager" groups.
    auto *actions = new QWidget(this);
    auto *actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(BarGap);

    m_installButton = new PillButton(PillButton::Accent, QString(), actions);
    connect(m_installButton, &PillButton::clicked, this, &AppsPage::installSelected);
    actionsLayout->addWidget(m_installButton);

    m_uninstallButton = new PillButton(PillButton::Ghost, QString(), actions);
    connect(m_uninstallButton, &PillButton::clicked, this, &AppsPage::uninstallSelected);
    actionsLayout->addWidget(m_uninstallButton);

    m_upgradeButton = new PillButton(PillButton::Ghost, QString(), actions);
    connect(m_upgradeButton, &PillButton::clicked, this, &AppsPage::upgradeAll);
    actionsLayout->addWidget(m_upgradeButton);

    actionsLayout->addStretch(1);

    m_managerLabel = new QLabel(actions);
    actionsLayout->addWidget(m_managerLabel);
    m_managerControl = new SegmentedControl({QStringLiteral("WinGet"), QStringLiteral("Chocolatey")}, actions);
    m_managerControl->setCurrentIndex(m_preference == Apps::Manager::Choco ? 1 : 0);
    connect(m_managerControl, &SegmentedControl::currentIndexChanged, this,
            [this](int index) { setPreference(index == 1 ? Apps::Manager::Choco : Apps::Manager::Winget); });
    actionsLayout->addWidget(m_managerControl);
    m_layout->addWidget(actions);

    // Row two: the selection tools — the "Selection" group — and the preset file.
    auto *selection = new QWidget(this);
    auto *selectionLayout = new QHBoxLayout(selection);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    selectionLayout->setSpacing(BarGap);

    m_selectedButton = new PillButton(PillButton::Ghost, QString(), selection);
    connect(m_selectedButton, &PillButton::clicked, this, &AppsPage::showSelectionPopup);
    selectionLayout->addWidget(m_selectedButton);

    m_clearButton = new PillButton(PillButton::Ghost, QString(), selection);
    connect(m_clearButton, &PillButton::clicked, this, &AppsPage::clearSelection);
    selectionLayout->addWidget(m_clearButton);

    m_installedButton = new PillButton(PillButton::Ghost, QString(), selection);
    connect(m_installedButton, &PillButton::clicked, this, &AppsPage::showInstalled);
    selectionLayout->addWidget(m_installedButton);

    m_collapseButton = new PillButton(PillButton::Ghost, QString(), selection);
    connect(m_collapseButton, &PillButton::clicked, this, [this] { setAllCollapsed(true); });
    selectionLayout->addWidget(m_collapseButton);

    m_expandButton = new PillButton(PillButton::Ghost, QString(), selection);
    connect(m_expandButton, &PillButton::clicked, this, [this] { setAllCollapsed(false); });
    selectionLayout->addWidget(m_expandButton);

    selectionLayout->addStretch(1);

    m_importButton = new PillButton(PillButton::Ghost, QString(), selection);
    connect(m_importButton, &PillButton::clicked, this, &AppsPage::importSelection);
    selectionLayout->addWidget(m_importButton);

    m_exportButton = new PillButton(PillButton::Ghost, QString(), selection);
    connect(m_exportButton, &PillButton::clicked, this, &AppsPage::exportSelection);
    selectionLayout->addWidget(m_exportButton);
    m_layout->addWidget(selection);

    // Row three: what the probe found, the repair button when something is missing, and
    // the log toggle.
    auto *status = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(status);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(BarGap);

    m_statusLabel = new QLabel(status);
    statusLayout->addWidget(m_statusLabel);

    m_repairButton = new PillButton(PillButton::Ghost, QString(), status);
    connect(m_repairButton, &PillButton::clicked, this, &AppsPage::repairManager);
    m_repairButton->hide();
    statusLayout->addWidget(m_repairButton);

    statusLayout->addStretch(1);

    m_logButton = new PillButton(PillButton::Ghost, QString(), status);
    connect(m_logButton, &PillButton::clicked, this, [this] {
        m_log->setVisible(!m_log->isVisible());
        retranslate();
    });
    statusLayout->addWidget(m_logButton);
    m_layout->addWidget(status);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(4000);
    m_log->setMaximumHeight(LogMaxHeight);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_log->setStyleSheet(logStyle());
    m_log->hide();
    m_layout->addWidget(m_log);

    m_progress = new ProgressStrip(this);
    m_progress->hide();
    m_layout->addWidget(m_progress);
}

void AppsPage::buildFilters()
{
    auto *row = new QWidget(this);
    auto *rowLayout = new QVBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(BarGap);

    m_search = new SearchField(row);
    m_search->setPlaceholderKey(QStringLiteral("apps.search.placeholder"));
    m_search->setShortcutBadgeVisible(false);
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(SearchDebounceMs);
    connect(m_searchTimer, &QTimer::timeout, this, &AppsPage::applyFilter);
    connect(m_search, &SearchField::textChanged, this, [this] { m_searchTimer->start(); });
    rowLayout->addWidget(m_search);

    auto *chips = new QWidget(row);
    auto *chipFlow = new FlowLayout(chips, 6, 6);
    m_filterHint = new QLabel(chips);
    chipFlow->addWidget(m_filterHint);

    QStringList categories = Apps::Catalogue::instance().categories();
    categories.prepend(QString());   // the All chip
    for (const QString &category : std::as_const(categories)) {
        auto *chip = new Chip(category, chips);
        chip->onClick = [this](const QString &c, bool additive) { onChipClicked(c, additive); };
        chipFlow->addWidget(chip);
        m_chips.append(chip);
    }
    rowLayout->addWidget(chips);

    m_fossLegend = new QLabel(row);
    rowLayout->addWidget(m_fossLegend);

    m_layout->addWidget(row);
}

void AppsPage::buildList()
{
    const Apps::Catalogue &catalogue = Apps::Catalogue::instance();

    m_body = new QWidget(this);
    auto *body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(BlockGap);

    for (const QString &category : catalogue.categories()) {
        Block block;
        block.category = category;
        block.header = new CategoryHeader(m_body);
        body->addWidget(block.header);

        block.panel = new QWidget(m_body);
        block.flow = new FlowLayout(block.panel, TileGapX, TileGapY);
        block.panel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        body->addWidget(block.panel);

        for (const Apps::Entry &e : catalogue.entries()) {
            if (e.category != category)
                continue;
            auto *tile = new AppEntry(e.key, e.name, e.foss, block.panel);
            connect(tile, &AppEntry::toggled, this, &AppsPage::setSelected);
            connect(tile, &AppEntry::contextRequested, this, [this](const QString &key, const QPoint &pos) {
                const Apps::Entry *entry = Apps::Catalogue::instance().entry(key);
                m_entryPopup->open(key, entry ? entry->link : QString(), pos);
            });
            block.flow->addWidget(tile);
            block.entries.append(tile);
            m_entries.insert(e.key, tile);
        }

        m_blocks.append(block);
        const int index = int(m_blocks.size()) - 1;
        block.header->onClick = [this, index] { toggleBlock(m_blocks[index]); };
    }

    m_layout->addWidget(m_body);
}

void AppsPage::retranslate()
{
    m_installButton->setText(Locale::tr(QStringLiteral("apps.install")));
    m_uninstallButton->setText(Locale::tr(QStringLiteral("apps.uninstall")));
    m_upgradeButton->setText(Locale::tr(QStringLiteral("apps.upgradeAll")));
    m_managerLabel->setText(Locale::tr(QStringLiteral("apps.manager")));
    tint(m_managerLabel, Theme::Font::pageSub(), Theme::Color::TextFaint());
    m_clearButton->setText(Locale::tr(QStringLiteral("apps.clearSelection")));
    m_installedButton->setText(Locale::tr(QStringLiteral("apps.showInstalled")));
    m_collapseButton->setText(Locale::tr(QStringLiteral("apps.collapseAll")));
    m_expandButton->setText(Locale::tr(QStringLiteral("apps.expandAll")));
    m_importButton->setText(Locale::tr(QStringLiteral("apps.import")));
    m_exportButton->setText(Locale::tr(QStringLiteral("apps.export")));
    m_logButton->setText(Locale::tr(m_log->isVisible() ? QStringLiteral("apps.log.hide")
                                                       : QStringLiteral("apps.log.show")));
    m_selectedButton->setText(Locale::tr(QStringLiteral("apps.selected")).arg(selectedCount()));

    m_filterHint->setText(Locale::tr(QStringLiteral("apps.filter.hint")));
    tint(m_filterHint, Theme::Font::tweakDesc(), Theme::Color::TextFaint());
    for (Chip *chip : std::as_const(m_chips))
        // tr(), not content(): content() hands back the source text under Turkish, and
        // these sources are WinUtil's English. Every category key is in the table.
        chip->setText(chip->category().isEmpty()
                          ? Locale::tr(QStringLiteral("apps.filter.all"))
                          : Locale::tr(Apps::categoryKey(chip->category())));

    m_fossLegend->setText(QStringLiteral("● ") + Locale::tr(QStringLiteral("apps.foss")));
    tint(m_fossLegend, Theme::Font::tweakDesc(), QColor(19, 143, 83));

    for (Block &block : m_blocks) {
        block.header->setTitle(Locale::tr(Apps::categoryKey(block.category)));
        block.header->setCount(Locale::tr(QStringLiteral("apps.count")).arg(block.entries.size()));
    }

    const Apps::Catalogue &catalogue = Apps::Catalogue::instance();
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const Apps::Entry *e = catalogue.entry(it.key());
        if (!e)
            continue;
        // Get-WinUtilEntryToolTip: the description, then the preset key.
        QString tip = e->description;
        if (!tip.isEmpty())
            tip += QStringLiteral("\n\n");
        tip += Locale::tr(QStringLiteral("apps.presetKey")).arg(e->presetKey());
        it.value()->setToolTip(tip);
    }

    m_entryPopup->setTexts(Locale::tr(QStringLiteral("apps.menu.install")),
                           Locale::tr(QStringLiteral("apps.menu.uninstall")),
                           Locale::tr(QStringLiteral("apps.menu.info")));
    refreshStatusLine();
}

// ------------------------------------------------------------ selection ---

void AppsPage::setSelected(const QString &key, bool on)
{
    if (on)
        m_selected.insert(key);
    else
        m_selected.remove(key);
    if (AppEntry *tile = m_entries.value(key))
        tile->setChecked(on);
    selectionChanged();
}

void AppsPage::clearSelection()
{
    for (const QString &key : std::as_const(m_selected))
        if (AppEntry *tile = m_entries.value(key))
            tile->setChecked(false);
    m_selected.clear();
    selectionChanged();
}

void AppsPage::selectionChanged()
{
    m_selectedButton->setText(Locale::tr(QStringLiteral("apps.selected")).arg(selectedCount()));
    const bool any = selectedCount() > 0;
    const bool idle = !busy();
    m_installButton->setEnabledLook(any && idle);
    m_uninstallButton->setEnabledLook(any && idle);
    m_clearButton->setEnabledLook(any);
    m_exportButton->setEnabledLook(any);
    Q_EMIT stateChanged();
}

void AppsPage::showSelectionPopup()
{
    const Apps::Catalogue &catalogue = Apps::Catalogue::instance();
    QVector<QPair<QString, QString>> rows;
    for (const QString &key : std::as_const(m_selected)) {
        const Apps::Entry *e = catalogue.entry(key);
        rows.append({key, e ? e->name : key});
    }
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
        return a.second.localeAwareCompare(b.second) < 0;
    });
    m_selectionPopup->rebuild(rows, Locale::tr(QStringLiteral("apps.selected.empty")),
                              Locale::tr(QStringLiteral("apps.selected.remove")),
                              [this](const QString &key) {
                                  setSelected(key, false);
                                  showSelectionPopup();
                              });
    const QPoint below = m_selectedButton->mapToGlobal(QPoint(0, m_selectedButton->height() + 4));
    m_selectionPopup->move(below);
    m_selectionPopup->show();
}

// ------------------------------------------------------------ filtering ---

void AppsPage::onChipClicked(const QString &category, bool additive)
{
    // Set-WinUtilAppCategoryFilter.
    if (category.isEmpty()) {
        m_activeCategories.clear();
    } else if (additive) {
        if (m_activeCategories.contains(category))
            m_activeCategories.remove(category);
        else
            m_activeCategories.insert(category);
    } else if (m_activeCategories.size() == 1 && m_activeCategories.contains(category)) {
        m_activeCategories.clear();
    } else {
        m_activeCategories.clear();
        m_activeCategories.insert(category);
    }
    syncChips();
    applyFilter();
}

void AppsPage::syncChips()
{
    for (Chip *chip : std::as_const(m_chips))
        chip->setChecked(chip->category().isEmpty() ? m_activeCategories.isEmpty()
                                                    : m_activeCategories.contains(chip->category()));
}

void AppsPage::applyFilter()
{
    // Find-AppsByNameOrDescription: both filters have to pass; a category with a match is
    // opened for as long as a filter is on, and put back as it was after.
    const QString needle = m_search->text().trimmed();
    const bool hasSearch = !needle.isEmpty();
    const bool hasCategories = !m_activeCategories.isEmpty();
    const Apps::Catalogue &catalogue = Apps::Catalogue::instance();

    for (Block &block : m_blocks) {
        if (!hasSearch && !hasCategories) {
            for (AppEntry *tile : std::as_const(block.entries))
                tile->show();
            if (m_autoExpanded.contains(block.category)) {
                block.collapsed = true;
                m_autoExpanded.remove(block.category);
            }
            block.header->setCollapsed(block.collapsed);
            block.header->show();
            block.panel->setVisible(!block.collapsed);
            continue;
        }

        bool any = false;
        for (AppEntry *tile : std::as_const(block.entries)) {
            const Apps::Entry *e = catalogue.entry(tile->key());
            const bool categoryMatch = !hasCategories || (e && m_activeCategories.contains(e->category));
            const bool textMatch = !hasSearch
                                   || (e && (e->name.contains(needle, Qt::CaseInsensitive)
                                             || e->description.contains(needle, Qt::CaseInsensitive)
                                             || e->key.contains(needle, Qt::CaseInsensitive)));
            const bool shown = e && categoryMatch && textMatch;
            tile->setVisible(shown);
            any = any || shown;
        }
        if (any) {
            if (block.collapsed) {
                block.collapsed = false;
                m_autoExpanded.insert(block.category);
            }
            block.header->setCollapsed(false);
            block.header->show();
            block.panel->show();
        } else {
            block.header->hide();
            block.panel->hide();
        }
    }
    for (Block &block : m_blocks)
        block.panel->updateGeometry();
    m_body->updateGeometry();
}

void AppsPage::setAllCollapsed(bool collapsed)
{
    // Invoke-WPFToggleAllCategories.
    for (Block &block : m_blocks) {
        block.collapsed = collapsed;
        m_autoExpanded.remove(block.category);
        block.header->setCollapsed(collapsed);
        if (block.header->isVisible())
            block.panel->setVisible(!collapsed);
    }
}

void AppsPage::toggleBlock(Block &block)
{
    m_autoExpanded.remove(block.category);   // an explicit click wins over the filter
    block.collapsed = !block.collapsed;
    block.header->setCollapsed(block.collapsed);
    block.panel->setVisible(!block.collapsed);
}

// ----------------------------------------------------------------- runs ---

QVector<Apps::Entry> AppsPage::selectedEntries() const
{
    const Apps::Catalogue &catalogue = Apps::Catalogue::instance();
    QVector<Apps::Entry> out;
    // In catalogue order, so the run reads top to bottom like the page.
    for (const Apps::Entry &e : catalogue.entries())
        if (m_selected.contains(e.key))
            out.append(e);
    return out;
}

void AppsPage::setPreference(Apps::Manager m)
{
    if (m_preference == m)
        return;
    m_preference = m;
    QSettings().setValue(KeyManager, Apps::managerToString(m));
    refreshStatusLine();
}

void AppsPage::installSelected()
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    const QVector<Apps::Entry> packages = selectedEntries();
    if (packages.isEmpty()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.selectInstall")));
        return;
    }
    runBatch(packages, Apps::Operation::Install);
}

void AppsPage::uninstallSelected()
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    const QVector<Apps::Entry> packages = selectedEntries();
    if (packages.isEmpty()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.selectUninstall")));
        return;
    }
    runBatch(packages, Apps::Operation::Uninstall);
}

void AppsPage::installOne(const QString &key)
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    if (const Apps::Entry *e = Apps::Catalogue::instance().entry(key))
        runBatch({*e}, Apps::Operation::Install);
}

void AppsPage::uninstallOne(const QString &key)
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    if (const Apps::Entry *e = Apps::Catalogue::instance().entry(key))
        runBatch({*e}, Apps::Operation::Uninstall);
}

void AppsPage::runBatch(const QVector<Apps::Entry> &packages, Apps::Operation op)
{
    const Apps::Split split = Apps::split(packages, m_preference);
    if (split.isEmpty()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.noPackageId")));
        return;
    }

    // What the dialog lists: the names, and past a dozen, how many more.
    QStringList names;
    for (const Apps::Entry &e : packages)
        names << e.name;
    QString list;
    if (names.size() > 12) {
        list = QStringList(names.mid(0, 12)).join(QStringLiteral(" · "))
               + Locale::tr(QStringLiteral("apps.confirm.more")).arg(names.size() - 12);
    } else {
        list = names.join(QStringLiteral(" · "));
    }

    const bool install = op == Apps::Operation::Install;
    const QString manager = m_preference == Apps::Manager::Choco ? QStringLiteral("Chocolatey")
                                                                 : QStringLiteral("WinGet");
    const QString title = install
                              ? Locale::tr(QStringLiteral("apps.confirm.install.title")).arg(packages.size())
                              : Locale::tr(QStringLiteral("apps.confirm.uninstall.title"));
    const QString body = install
                             ? Locale::tr(QStringLiteral("apps.confirm.install.body")).arg(manager)
                                   + QStringLiteral("\n\n") + list
                             : Locale::tr(QStringLiteral("apps.confirm.uninstall.body")).arg(packages.size())
                                   + QStringLiteral("\n\n") + list;

    const bool go = Dialog::confirm(this, title, body,
                                    Locale::tr(install ? QStringLiteral("apps.install")
                                                       : QStringLiteral("apps.uninstall")),
                                    Locale::tr(QStringLiteral("actions.cancel")),
                                    Apps::commandSummary(split, op));
    if (!go)
        return;

    // Which tiles a package id speaks for, so progress can land on the right one.
    m_idToKeys.clear();
    m_pendingKeys.clear();
    for (const Apps::Entry &e : packages) {
        m_pendingKeys << e.key;
        if (e.hasWinget())
            m_idToKeys[e.winget].append(e.key);
        if (e.hasChoco())
            m_idToKeys[e.choco].append(e.key);
        if (AppEntry *tile = m_entries.value(e.key))
            tile->setStatus(QString(), true);
    }

    if (install)
        m_runner->install(split);
    else
        m_runner->uninstall(split);
}

void AppsPage::upgradeAll()
{
    QString error;
    if (m_runner->upgradeAll(m_preference, &error))
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.upgradeStarted")));
    else
        Q_EMIT notice(Locale::tr(error == QLatin1String("powershell")
                                     ? QStringLiteral("err.noPowerShell")
                                     : QStringLiteral("apps.notice.upgradeFailed")));
}

void AppsPage::showInstalled()
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    // Invoke-WPFGetInstalled returns without a word when WinGet is the preference and it
    // is not there; a sentence is kinder.
    if (m_probed && m_preference == Apps::Manager::Winget && !m_wingetPresent) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.wingetMissing")));
        return;
    }
    m_runner->detectInstalled(m_preference);
}

void AppsPage::repairManager()
{
    if (busy()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.busy")));
        return;
    }
    const Apps::Manager m = m_preference;
    const QString what = m == Apps::Manager::Choco ? QStringLiteral("Chocolatey") : QStringLiteral("WinGet");
    const bool go = Dialog::confirm(
        this, Locale::tr(QStringLiteral("apps.repair.title")).arg(what),
        Locale::tr(m == Apps::Manager::Choco ? QStringLiteral("apps.repair.choco.body")
                                             : QStringLiteral("apps.repair.winget.body")),
        Locale::tr(QStringLiteral("actions.run")), Locale::tr(QStringLiteral("actions.cancel")),
        m == Apps::Manager::Choco
            ? QStringLiteral("Invoke-WebRequest -Uri https://community.chocolatey.org/install.ps1 "
                             "-UseBasicParsing | Invoke-Expression")
            : QStringLiteral("Install-PackageProvider -Name NuGet -Force\n"
                             "Install-Module -Name Microsoft.WinGet.Client -Force\n"
                             "Repair-WinGetPackageManager -AllUsers"));
    if (!go)
        return;
    m_runner->repair(m);
}

void AppsPage::importSelection()
{
    // Invoke-WPFImpex -type import, for the install keys.
    const QString path = QFileDialog::getOpenFileName(
        this, Locale::tr(QStringLiteral("apps.import")),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        Locale::tr(QStringLiteral("apps.file.filter")));
    if (path.isEmpty())
        return;

    QStringList keys;
    int unknown = 0;
    QString error;
    if (!Apps::importSelection(path, &keys, &unknown, &error)) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.importFailed")).arg(error));
        return;
    }
    if (keys.isEmpty() && unknown == 0) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.importEmpty")));
        return;
    }
    // A preset replaces the selection rather than merging with it (Invoke-WPFPresets).
    clearSelection();
    for (const QString &key : std::as_const(keys)) {
        m_selected.insert(key);
        if (AppEntry *tile = m_entries.value(key))
            tile->setChecked(true);
    }
    selectionChanged();
    QString text = Locale::tr(QStringLiteral("apps.notice.imported")).arg(keys.size());
    if (unknown > 0)
        text += Locale::tr(QStringLiteral("apps.notice.importedUnknown")).arg(unknown);
    Q_EMIT notice(text);
}

void AppsPage::exportSelection()
{
    if (m_selected.isEmpty()) {
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.exportEmpty")));
        return;
    }
    const QString suggested = QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))
                                  .filePath(QStringLiteral("arbitrium-apps.json"));
    const QString path = QFileDialog::getSaveFileName(
        this, Locale::tr(QStringLiteral("apps.export")), suggested,
        Locale::tr(QStringLiteral("apps.file.filter")));
    if (path.isEmpty())
        return;

    QStringList keys;
    for (const Apps::Entry &e : selectedEntries())
        keys << e.key;
    QString error;
    if (Apps::exportSelection(path, keys, &error))
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.exported")).arg(keys.size()));
    else
        Q_EMIT notice(Locale::tr(QStringLiteral("apps.notice.exportFailed")).arg(error));
}

void AppsPage::openLink(const QString &key)
{
    const Apps::Entry *e = Apps::Catalogue::instance().entry(key);
    if (!e || e->link.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl(e->link));
}

// ------------------------------------------------------- runner feedback ---

QStringList AppsPage::keysForId(const QString &id) const
{
    // A choco batch reports every id at once, joined with commas.
    QStringList keys;
    for (const QString &one : id.split(QLatin1Char(','), Qt::SkipEmptyParts))
        keys += m_idToKeys.value(one.trimmed());
    return keys;
}

QString AppsPage::describeProgress(const QString &id, bool finished, int done, int total) const
{
    const bool install = m_runner->job() == Apps::Runner::Job::Install;
    // A choco batch arrives as every id joined with commas; a single choco id is one that
    // some selected row carries as its choco id and not as its winget id.
    bool choco = id.contains(QLatin1Char(','));
    if (!choco) {
        for (const QString &key : m_idToKeys.value(id)) {
            const Apps::Entry *e = Apps::Catalogue::instance().entry(key);
            if (e && e->choco == id && e->winget != id)
                choco = true;
        }
    }
    const int shown = finished ? done : done + 1;

    if (choco) {
        const QString key = install
                                ? (finished ? QStringLiteral("apps.progress.install.chocoDone")
                                            : QStringLiteral("apps.progress.install.chocoRunning"))
                                : (finished ? QStringLiteral("apps.progress.uninstall.chocoDone")
                                            : QStringLiteral("apps.progress.uninstall.chocoRunning"));
        return Locale::tr(key).arg(shown).arg(total);
    }

    // The name when the id maps to one tile, the id itself otherwise — WinUtil shows the id.
    QString name = id;
    const QStringList keys = m_idToKeys.value(id);
    if (keys.size() == 1)
        if (const Apps::Entry *e = Apps::Catalogue::instance().entry(keys.first()))
            name = e->name;

    const QString key = install
                            ? (finished ? QStringLiteral("apps.progress.install.done")
                                        : QStringLiteral("apps.progress.install.running"))
                            : (finished ? QStringLiteral("apps.progress.uninstall.done")
                                        : QStringLiteral("apps.progress.uninstall.running"));
    return Locale::tr(key).arg(name).arg(shown).arg(total);
}

void AppsPage::setRunControlsEnabled(bool enabled)
{
    const bool any = selectedCount() > 0;
    m_installButton->setEnabledLook(enabled && any);
    m_uninstallButton->setEnabledLook(enabled && any);
    m_installedButton->setEnabledLook(enabled);
    m_repairButton->setEnabledLook(enabled);
    m_importButton->setEnabledLook(enabled);
}

void AppsPage::onStarted(Apps::Runner::Job job, int total)
{
    using Job = Apps::Runner::Job;
    if (job == Job::Probe)
        return;

    setRunControlsEnabled(false);
    m_log->clear();

    switch (job) {
    case Job::Install:
    case Job::Uninstall:
        for (const QString &key : std::as_const(m_pendingKeys))
            if (AppEntry *tile = m_entries.value(key))
                tile->setBusy(true);
        m_progressText = Locale::tr(job == Job::Install
                                        ? QStringLiteral("apps.progress.install.preparing")
                                        : QStringLiteral("apps.progress.uninstall.preparing"))
                             .arg(total);
        break;
    case Job::Detect:
        m_progressText = Locale::tr(QStringLiteral("apps.progress.detecting"));
        break;
    case Job::Repair:
        m_progressText = Locale::tr(QStringLiteral("apps.progress.repairing"));
        break;
    case Job::Probe:
    case Job::None:
        break;
    }
    m_progress->set(m_progressText, 0, false);
    m_progress->show();
    Q_EMIT stateChanged();
}

void AppsPage::onProgress(int done, int total, const QString &id, bool finished, int exitCode)
{
    const int percent = total > 0 ? int(100.0 * done / total) : 0;
    m_progressText = describeProgress(id, finished, done, total);
    m_progress->set(m_progressText, percent, false);

    if (finished) {
        const bool install = m_runner->job() == Apps::Runner::Job::Install;
        QString word;
        bool good = true;
        if (exitCode == 0) {
            word = Locale::tr(install ? QStringLiteral("apps.tile.installed")
                                      : QStringLiteral("apps.tile.uninstalled"));
        } else if (exitCode == -1978335189 || exitCode == -1978335135) {
            word = Locale::tr(QStringLiteral("apps.tile.current"));
        } else {
            word = Locale::tr(QStringLiteral("apps.tile.exit")).arg(exitCode);
            good = false;
        }
        for (const QString &key : keysForId(id)) {
            if (AppEntry *tile = m_entries.value(key)) {
                tile->setStatus(word, good);
                tile->setBusy(false);
            }
        }
    }
    Q_EMIT stateChanged();
}

void AppsPage::onLine(const QString &text)
{
    m_log->appendPlainText(text);
}

void AppsPage::onFinished(Apps::Runner::Job job, bool ok, int failures)
{
    using Job = Apps::Runner::Job;
    if (job == Job::Probe)
        return;

    for (const QString &key : std::as_const(m_pendingKeys))
        if (AppEntry *tile = m_entries.value(key))
            tile->setBusy(false);

    switch (job) {
    case Job::Install:
    case Job::Uninstall: {
        const bool install = job == Job::Install;
        const bool failed = !ok || failures > 0;
        m_progressText = Locale::tr(install ? (failed ? QStringLiteral("apps.progress.install.failed")
                                                      : QStringLiteral("apps.progress.install.finished"))
                                            : (failed ? QStringLiteral("apps.progress.uninstall.failed")
                                                      : QStringLiteral("apps.progress.uninstall.finished")));
        m_progress->set(m_progressText, 100, failed);
        QString text = Locale::tr(QStringLiteral("apps.notice.batchDone"))
                           .arg(qMax(0, m_pendingKeys.size() - failures))
                           .arg(m_pendingKeys.size());
        if (failures > 0)
            text += Locale::tr(QStringLiteral("apps.notice.batchFailures")).arg(failures);
        if (!ok && failures == 0)
            text = Locale::tr(QStringLiteral("apps.notice.batchAborted"));
        Q_EMIT notice(text);
        m_pendingKeys.clear();
        break;
    }
    case Job::Detect:
        // onDetected already spoke.
        m_progress->hide();
        break;
    case Job::Repair:
        m_progress->hide();
        Q_EMIT notice(Locale::tr(ok ? QStringLiteral("apps.notice.repaired")
                                    : QStringLiteral("apps.notice.repairFailed")));
        m_runner->probe();
        break;
    case Job::Probe:
    case Job::None:
        break;
    }

    m_progressText.clear();
    setRunControlsEnabled(true);
    selectionChanged();
}

void AppsPage::onDetected(const QStringList &keys)
{
    // Invoke-WPFGetInstalled adds to the selection rather than replacing it.
    for (const QString &key : keys) {
        m_selected.insert(key);
        if (AppEntry *tile = m_entries.value(key))
            tile->setChecked(true);
    }
    selectionChanged();
    Q_EMIT notice(keys.isEmpty() ? Locale::tr(QStringLiteral("apps.notice.installedNone"))
                                 : Locale::tr(QStringLiteral("apps.notice.installedFound")).arg(keys.size()));
}

void AppsPage::onProbed(bool winget, bool choco)
{
    m_probed = true;
    m_wingetPresent = winget;
    m_chocoPresent = choco;
    refreshStatusLine();
}

void AppsPage::refreshStatusLine()
{
    const QString checking = Locale::tr(QStringLiteral("apps.status.checking"));
    const QString installed = Locale::tr(QStringLiteral("apps.status.installed"));
    const QString missing = Locale::tr(QStringLiteral("apps.status.missing"));
    const QString w = !m_probed ? checking : (m_wingetPresent ? installed : missing);
    const QString c = !m_probed ? checking : (m_chocoPresent ? installed : missing);
    m_statusLabel->setText(QStringLiteral("WinGet: %1 · Chocolatey: %2").arg(w, c));
    tint(m_statusLabel, Theme::Font::pageSub(), Theme::Color::TextFaint());

    const bool needRepair = m_probed && (m_preference == Apps::Manager::Choco ? !m_chocoPresent : !m_wingetPresent);
    m_repairButton->setText(Locale::tr(m_preference == Apps::Manager::Choco
                                           ? QStringLiteral("apps.repair.choco")
                                           : QStringLiteral("apps.repair.winget")));
    m_repairButton->setVisible(needRepair);
}
