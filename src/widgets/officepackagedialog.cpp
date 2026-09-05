#include "officepackagedialog.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"
#include "buttons.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QVBoxLayout>

namespace {

struct ChannelEntry {
    const char *id;
    const char *name;
};

const ChannelEntry Channels[] = {
    {"492350f6-3a01-4f97-b9c0-c7c6ddf67d60", "Current Channel"},
    {"55336b82-a18d-4dd6-b5f6-9e5095c314a6", "Monthly Enterprise Channel"},
    {"7ffbc6bf-bc32-4f92-8982-f9dd17fd3114", "Semi-Annual Enterprise Channel"},
    {"c02d8fe6-5242-4da8-972f-82ee55e00671", "Office 2024 Perpetual Enterprise (LTSC)"},
    {"5030841d-c919-4594-8d2d-84ae4f96e58e", "Office 2021 Perpetual Enterprise (LTSC)"},
    {"f2e724c1-748f-4b47-8fb8-8e0d210e9208", "Office 2019 Perpetual Enterprise"},
    {"5440fd1f-7ecb-4221-8110-145efaa6372f", "Beta Channel (Insider Fast)"},
    {"64256afe-f5d9-4f86-8936-8840a6a4f5be", "Current Channel (Preview)"}
};

struct LangEntry {
    const char *tag;
    const char *name;
};

const LangEntry Languages[] = {
    {"tr-tr", "Türkçe (Turkish)"},
    {"en-us", "English (United States)"},
    {"en-gb", "English (United Kingdom)"},
    {"de-de", "Deutsch (German)"},
    {"fr-fr", "Français (French)"},
    {"es-es", "Español (Spanish)"},
    {"it-it", "Italiano (Italian)"},
    {"pt-br", "Português (Brasil)"},
    {"pt-pt", "Português (Portugal)"},
    {"pl-pl", "Polski (Polish)"},
    {"ru-ru", "Русский (Russian)"},
    {"ar-sa", "العربية (Arabic)"},
    {"zh-cn", "Chinese (Simplified)"},
    {"zh-tw", "Chinese (Traditional)"},
    {"ja-jp", "Japanese"},
    {"ko-kr", "Korean"},
    {"nl-nl", "Dutch"},
    {"sv-se", "Swedish"},
    {"da-dk", "Danish"},
    {"fi-fi", "Finnish"},
    {"nb-no", "Norwegian"},
    {"cs-cz", "Czech"},
    {"el-gr", "Greek"},
    {"hu-hu", "Hungarian"},
    {"ro-ro", "Romanian"},
    {"uk-ua", "Ukrainian"},
    {"vi-vn", "Vietnamese"},
    {"th-th", "Thai"},
    {"hi-in", "Hindi"},
    {"he-il", "Hebrew"},
    {"id-id", "Indonesian"}
};

} // namespace

OfficePackageDialog::OfficePackageDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(660);

    setupUi();
    updateFreeSpace();
    retranslate();
}

void OfficePackageDialog::setupUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->setSpacing(12);

    // Başlık
    auto *titleLabel = new QLabel(Locale::tr(QStringLiteral("office.dialog.pkg_title")), this);
    titleLabel->setFont(Theme::sans(15.0, Theme::Weight::SemiBold));
    titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextPrimary().name(QColor::HexRgb)));
    outer->addWidget(titleLabel);

    // 1. Satır: Kanal (Sol) ve Mimari (Sağ)
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(12);

    auto *chCol = new QVBoxLayout();
    chCol->setSpacing(4);
    m_channelLabel = new QLabel(this);
    m_channelLabel->setFont(Theme::Font::tweakDesc());
    chCol->addWidget(m_channelLabel);
    m_channelCombo = new QComboBox(this);
    m_channelCombo->setFixedHeight(32);
    for (const auto &ch : Channels) {
        m_channelCombo->addItem(QString::fromUtf8(ch.name), QString::fromLatin1(ch.id));
    }
    chCol->addWidget(m_channelCombo);
    row1->addLayout(chCol, 1);

    auto *archCol = new QVBoxLayout();
    archCol->setSpacing(4);
    m_archLabel = new QLabel(this);
    m_archLabel->setFont(Theme::Font::tweakDesc());
    archCol->addWidget(m_archLabel);
    m_archCombo = new QComboBox(this);
    m_archCombo->setFixedHeight(32);
    m_archCombo->addItem(Locale::tr(QStringLiteral("office.dialog.arch.x64")), QStringLiteral("x64"));
    m_archCombo->addItem(Locale::tr(QStringLiteral("office.dialog.arch.x86")), QStringLiteral("x86"));
    m_archCombo->addItem(Locale::tr(QStringLiteral("office.dialog.arch.multi")), QStringLiteral("Multi"));
    archCol->addWidget(m_archCombo);
    row1->addLayout(archCol, 1);

    outer->addLayout(row1);

    // 2. Satır: Dil (Sol) ve İndirme Aracı (Sağ)
    auto *row2 = new QHBoxLayout();
    row2->setSpacing(12);

    auto *langCol = new QVBoxLayout();
    langCol->setSpacing(4);
    m_langLabel = new QLabel(this);
    m_langLabel->setFont(Theme::Font::tweakDesc());
    langCol->addWidget(m_langLabel);
    m_langCombo = new QComboBox(this);
    m_langCombo->setFixedHeight(32);
    int defaultLangIdx = 0;
    const QString currentAppLang = Locale::language();
    for (int i = 0; i < int(sizeof(Languages) / sizeof(Languages[0])); ++i) {
        const auto &lang = Languages[i];
        m_langCombo->addItem(QString::fromUtf8(lang.name), QString::fromLatin1(lang.tag));
        if (QString::fromLatin1(lang.tag).startsWith(currentAppLang)) {
            defaultLangIdx = i;
        }
    }
    m_langCombo->setCurrentIndex(defaultLangIdx);
    langCol->addWidget(m_langCombo);
    row2->addLayout(langCol, 1);

    auto *toolCol = new QVBoxLayout();
    toolCol->setSpacing(4);
    m_toolLabel = new QLabel(this);
    m_toolLabel->setFont(Theme::Font::tweakDesc());
    toolCol->addWidget(m_toolLabel);
    m_toolCombo = new QComboBox(this);
    m_toolCombo->setFixedHeight(32);
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.aria2")), QStringLiteral("aria2"));
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.curl")), QStringLiteral("curl"));
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.wget")), QStringLiteral("wget"));
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.internal")), QStringLiteral("internal"));
    toolCol->addWidget(m_toolCombo);
    row2->addLayout(toolCol, 1);

    outer->addLayout(row2);

    // 3. Hedef Konum
    auto *pathCol = new QVBoxLayout();
    pathCol->setSpacing(4);
    m_pathLabel = new QLabel(this);
    m_pathLabel->setFont(Theme::Font::tweakDesc());
    pathCol->addWidget(m_pathLabel);

    auto *pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(8);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setFixedHeight(32);
    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QStringLiteral("/Office");
    m_pathEdit->setText(defaultPath);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &OfficePackageDialog::updateFreeSpace);
    pathLayout->addWidget(m_pathEdit, 1);

    m_browseBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.dialog.browse")), this);
    connect(m_browseBtn, &PillButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, Locale::tr(QStringLiteral("office.dialog.path")), m_pathEdit->text());
        if (!dir.isEmpty()) {
            m_pathEdit->setText(dir);
        }
    });
    pathLayout->addWidget(m_browseBtn);
    pathCol->addLayout(pathLayout);
    outer->addLayout(pathCol);

    // Boş alan etiketi
    m_freeSpaceLabel = new QLabel(this);
    m_freeSpaceLabel->setFont(Theme::sans(10.5, Theme::Weight::Regular));
    outer->addWidget(m_freeSpaceLabel);

    outer->addSpacing(8);

    // 4. Eylem Butonları
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(10);
    btnLayout->addStretch(1);

    m_cancelBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.dialog.cancel")), this);
    connect(m_cancelBtn, &PillButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    m_startBtn = new PillButton(PillButton::Accent, Locale::tr(QStringLiteral("office.dialog.start")), this);
    connect(m_startBtn, &PillButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_startBtn);

    outer->addLayout(btnLayout);
}

void OfficePackageDialog::updateFreeSpace()
{
    QString checkPath = m_pathEdit->text();
    while (!checkPath.isEmpty() && !QDir(checkPath).exists()) {
        const QString parent = QFileInfo(checkPath).dir().absolutePath();
        if (parent == checkPath)
            break;
        checkPath = parent;
    }
    QStorageInfo storage(checkPath);
    if (storage.isValid()) {
        const double freeGb = double(storage.bytesAvailable()) / (1024.0 * 1024.0 * 1024.0);
        m_freeSpaceLabel->setText(Locale::tr(QStringLiteral("office.dialog.free_space")).arg(freeGb, 0, 'f', 1));
        m_freeSpaceLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));
    } else {
        m_freeSpaceLabel->setText(QString());
    }
}

void OfficePackageDialog::retranslate()
{
    m_channelLabel->setText(Locale::tr(QStringLiteral("office.dialog.channel")));
    m_archLabel->setText(Locale::tr(QStringLiteral("office.dialog.arch")));
    m_langLabel->setText(Locale::tr(QStringLiteral("office.dialog.language")));
    m_toolLabel->setText(Locale::tr(QStringLiteral("office.dialog.tool")));
    m_pathLabel->setText(Locale::tr(QStringLiteral("office.dialog.path")));
    m_browseBtn->setText(Locale::tr(QStringLiteral("office.dialog.browse")));
    m_startBtn->setText(Locale::tr(QStringLiteral("office.dialog.start")));
    m_cancelBtn->setText(Locale::tr(QStringLiteral("office.dialog.cancel")));

    const int curArch = (m_archCombo->currentIndex() >= 0) ? m_archCombo->currentIndex() : 0;
    m_archCombo->clear();
    m_archCombo->addItem(Locale::tr(QStringLiteral("office.dialog.arch.x64")), QStringLiteral("x64"));
    m_archCombo->addItem(Locale::tr(QStringLiteral("office.dialog.arch.x86")), QStringLiteral("x86"));
    m_archCombo->addItem(Locale::tr(QStringLiteral("office.dialog.arch.multi")), QStringLiteral("Multi"));
    m_archCombo->setCurrentIndex(curArch);

    const int curTool = (m_toolCombo->currentIndex() >= 0) ? m_toolCombo->currentIndex() : 0;
    m_toolCombo->clear();
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.aria2")), QStringLiteral("aria2"));
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.curl")), QStringLiteral("curl"));
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.wget")), QStringLiteral("wget"));
    m_toolCombo->addItem(Locale::tr(QStringLiteral("office.dialog.tool.internal")), QStringLiteral("internal"));
    m_toolCombo->setCurrentIndex(curTool);

    const QString inputStyle = QStringLiteral(
        "QComboBox, QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px 8px; }"
        "QComboBox:hover, QLineEdit:hover { border-color: %4; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %1; color: %2; selection-background-color: %4; }")
        .arg(Theme::Color::Surface().name(QColor::HexRgb),
             Theme::Color::TextPrimary().name(QColor::HexRgb),
             Theme::Color::BorderControl().name(QColor::HexRgb),
             Theme::accent().name(QColor::HexRgb));

    m_channelCombo->setStyleSheet(inputStyle);
    m_archCombo->setStyleSheet(inputStyle);
    m_langCombo->setStyleSheet(inputStyle);
    m_toolCombo->setStyleSheet(inputStyle);
    m_pathEdit->setStyleSheet(inputStyle);

    m_channelLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));
    m_archLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));
    m_langLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));
    m_toolLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));
    m_pathLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));

    updateFreeSpace();
}

QString OfficePackageDialog::selectedChannelGuid() const
{
    return m_channelCombo->currentData().toString();
}

QString OfficePackageDialog::selectedChannelName() const
{
    return m_channelCombo->currentText();
}

QString OfficePackageDialog::selectedArch() const
{
    return m_archCombo->currentData().toString();
}

QString OfficePackageDialog::selectedLangTag() const
{
    return m_langCombo->currentData().toString();
}

QString OfficePackageDialog::selectedTool() const
{
    return m_toolCombo->currentData().toString();
}

QString OfficePackageDialog::selectedOutputDir() const
{
    return m_pathEdit->text();
}

void OfficePackageDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal rad = Theme::Metric::ControlRadius + 6.0;

    p.setPen(Theme::Color::BorderControl());
    p.setBrush(Theme::Color::Window());
    p.drawRoundedRect(r, rad, rad);
}

void OfficePackageDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (parentWidget()) {
        const QRect parentRect = parentWidget()->geometry();
        const int x = parentRect.x() + (parentRect.width() - width()) / 2;
        const int y = parentRect.y() + qMax(16, (parentRect.height() - height()) / 2);
        move(x, y);
    }
}
