#include "yaoctrudialog.h"

#include "../css.h"
#include "../i18n.h"
#include "../theme.h"
#include "buttons.h"

#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

struct ChannelEntry {
    const char *id;
    const char *name;
    const char *cmdTag;
    bool supportsWin7;
    bool supportsWin81;
};

const ChannelEntry Channels[] = {
    {"492350f6-3a01-4f97-b9c0-c7c6ddf67d60", "Current Channel", "Monthly", true, true},
    {"55336b82-a18d-4dd6-b5f6-9e5095c314a6", "Monthly Enterprise Channel", "MonthlyEnterprise", true, true},
    {"7ffbc6bf-bc32-4f92-8982-f9dd17fd3114", "Semi-Annual Enterprise Channel", "SemiAnnual", true, true},
    {"c02d8fe6-5242-4da8-972f-82ee55e00671", "Office 2024 Perpetual Enterprise (LTSC)", "PerpetualVL2024", false, false},
    {"5030841d-c919-4594-8d2d-84ae4f96e58e", "Office 2021 Perpetual Enterprise (LTSC)", "PerpetualVL2021", false, false},
    {"f2e724c1-748f-4b47-8fb8-8e0d210e9208", "Office 2019 Perpetual Enterprise", "PerpetualVL2019", false, false},
    {"5440fd1f-7ecb-4221-8110-145efaa6372f", "Beta Channel (Insider Fast)", "InsiderFast", true, false},
    {"64256afe-f5d9-4f86-8936-8840a6a4f5be", "Current Channel (Preview)", "MonthlyPreview", true, true}
};

struct LangEntry {
    const char *tag;
    const char *name;
    int lcid;
};

const LangEntry Languages[] = {
    {"tr-tr", "Türkçe (Turkish)", 1055},
    {"en-us", "English (United States)", 1033},
    {"en-gb", "English (United Kingdom)", 2057},
    {"de-de", "Deutsch (German)", 1031},
    {"fr-fr", "Français (French)", 1036},
    {"es-es", "Español (Spanish)", 3082},
    {"it-it", "Italiano (Italian)", 1040},
    {"pt-br", "Português (Brasil)", 1046},
    {"pt-pt", "Português (Portugal)", 2070},
    {"pl-pl", "Polski (Polish)", 1045},
    {"ru-ru", "Русский (Russian)", 1049},
    {"ar-sa", "العربية (Arabic)", 1025},
    {"zh-cn", "简体中文 (Simplified Chinese)", 2052},
    {"zh-tw", "繁體中文 (Traditional Chinese)", 1028},
    {"ja-jp", "日本語 (Japanese)", 1041},
    {"ko-kr", "한국어 (Korean)", 1042},
    {"nl-nl", "Nederlands (Dutch)", 1043},
    {"sv-se", "Svenska (Swedish)", 1053},
    {"da-dk", "Dansk (Danish)", 1030},
    {"fi-fi", "Suomi (Finnish)", 1035},
    {"nb-no", "Norsk Bokmål (Norwegian)", 1044},
    {"cs-cz", "Čeština (Czech)", 1029},
    {"el-gr", "Ελληνικά (Greek)", 1032},
    {"hu-hu", "Magyar (Hungarian)", 1038},
    {"ro-ro", "Română (Romanian)", 1048},
    {"uk-ua", "Українська (Ukrainian)", 1058},
    {"vi-vn", "Tiếng Việt (Vietnamese)", 1066},
    {"th-th", "ไทย (Thai)", 1054},
    {"hi-in", "हिन्दी (Hindi)", 1081},
    {"he-il", "עברית (Hebrew)", 1037},
    {"id-id", "Bahasa Indonesia (Indonesian)", 1057},
    {"es-mx", "Español (México)", 2058},
    {"fr-ca", "Français (Canada)", 3084},
    {"sk-sk", "Slovenčina (Slovak)", 1051}
};

static QString ensureAria2Extracted()
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Arbitrium");
    QDir().mkpath(tempDir);
    const QString aria2 = QDir(tempDir).filePath(QStringLiteral("aria2c.exe"));

    QFile resFile(QStringLiteral(":/bin/aria2c.exe"));
    if (resFile.exists()) {
        const qint64 resSize = resFile.size();
        if (!QFileInfo::exists(aria2) || QFileInfo(aria2).size() != resSize) {
            if (QFile::exists(aria2))
                QFile::remove(aria2);
            if (resFile.open(QIODevice::ReadOnly)) {
                QFile outFile(aria2);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(resFile.readAll());
                    outFile.close();
                }
                resFile.close();
            }
        }
    }
    return aria2;
}

static QString ensureWgetExtracted()
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Arbitrium");
    QDir().mkpath(tempDir);
    const QString wget = QDir(tempDir).filePath(QStringLiteral("wget.exe"));

    QFile resFile(QStringLiteral(":/bin/wget.exe"));
    if (resFile.exists()) {
        const qint64 resSize = resFile.size();
        if (!QFileInfo::exists(wget) || QFileInfo(wget).size() != resSize) {
            if (QFile::exists(wget))
                QFile::remove(wget);
            if (resFile.open(QIODevice::ReadOnly)) {
                QFile outFile(wget);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(resFile.readAll());
                    outFile.close();
                }
                resFile.close();
            }
        }
    }
    return wget;
}

static QString ensureCurlExtracted()
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Arbitrium");
    QDir().mkpath(tempDir);
    const QString curlPath = QDir(tempDir).filePath(QStringLiteral("curl.exe"));

    QFile resFile(QStringLiteral(":/bin/curl.exe"));
    if (resFile.exists()) {
        const qint64 resSize = resFile.size();
        if (!QFileInfo::exists(curlPath) || QFileInfo(curlPath).size() != resSize) {
            if (QFile::exists(curlPath))
                QFile::remove(curlPath);
            if (resFile.open(QIODevice::ReadOnly)) {
                QFile outFile(curlPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(resFile.readAll());
                    outFile.close();
                }
                resFile.close();
            }
        }
        return curlPath;
    }

    const QString sysCurl = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (!sysCurl.isEmpty() && QFileInfo::exists(sysCurl)) {
        return sysCurl;
    }
    const QString win32Curl = QStringLiteral("C:/Windows/System32/curl.exe");
    if (QFileInfo::exists(win32Curl)) {
        return win32Curl;
    }

    return QString();
}

} // namespace

QString YaoctruDialog::ensureCurlExtracted()
{
    return ::ensureCurlExtracted();
}

YaoctruDialog::YaoctruDialog(QWidget *parent)
    : QDialog(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(660);

    setupUi();
    updateFreeSpace();
    retranslate();

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, &YaoctruDialog::retranslate);
    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &YaoctruDialog::retranslate);
}

YaoctruDialog::~YaoctruDialog() = default;

void YaoctruDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 18, 24, 18);
    mainLayout->setSpacing(10);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setFont(Theme::sans(14.0, Theme::Weight::SemiBold));
    mainLayout->addWidget(m_titleLabel);

    m_descLabel = new QLabel(this);
    m_descLabel->setFont(Theme::Font::tweakDesc());
    m_descLabel->setWordWrap(true);
    mainLayout->addWidget(m_descLabel);

    auto makeLabel = [this](const QString &text) {
        auto *lbl = new QLabel(text, this);
        lbl->setFont(Theme::sans(10.5, Theme::Weight::Medium));
        lbl->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));
        return lbl;
    };

    // 1. OS Compatibility Level & Channel in two columns (Row 1)
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(12);

    auto *osCol = new QVBoxLayout();
    osCol->setSpacing(4);
    osCol->addWidget(makeLabel(Locale::tr(QStringLiteral("office.yaoctru.os_level"))));
    m_osCombo = new QComboBox(this);
    m_osCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.os_default")), QString());
    m_osCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.os_win81")), QStringLiteral("Win81"));
    m_osCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.os_win7")), QStringLiteral("Win7"));
    connect(m_osCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &YaoctruDialog::onOsLevelChanged);
    osCol->addWidget(m_osCombo);
    row1->addLayout(osCol, 1);

    auto *channelCol = new QVBoxLayout();
    channelCol->setSpacing(4);
    channelCol->addWidget(makeLabel(Locale::tr(QStringLiteral("office.dialog.channel"))));
    m_channelCombo = new QComboBox(this);
    for (const auto &c : Channels) {
        m_channelCombo->addItem(QString::fromUtf8(c.name), QString::fromLatin1(c.id));
    }
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &YaoctruDialog::onChannelChanged);
    channelCol->addWidget(m_channelCombo);
    row1->addLayout(channelCol, 1);

    mainLayout->addLayout(row1);

    // 2. Architecture & Product Type in two columns (Row 2)
    auto *row2 = new QHBoxLayout();
    row2->setSpacing(12);

    auto *archCol = new QVBoxLayout();
    archCol->setSpacing(4);
    archCol->addWidget(makeLabel(Locale::tr(QStringLiteral("office.dialog.arch"))));
    m_archCombo = new QComboBox(this);
    archCol->addWidget(m_archCombo);
    row2->addLayout(archCol, 1);

    auto *prodCol = new QVBoxLayout();
    prodCol->setSpacing(4);
    prodCol->addWidget(makeLabel(Locale::tr(QStringLiteral("office.yaoctru.product_type"))));
    m_productCombo = new QComboBox(this);
    m_productCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.type_full")), QStringLiteral("Full"));
    m_productCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.type_lang")), QStringLiteral("Lang"));
    m_productCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.type_proof")), QStringLiteral("Proof"));
    prodCol->addWidget(m_productCombo);
    row2->addLayout(prodCol, 1);

    mainLayout->addLayout(row2);

    updateArchCombo();

    // 3. Language & Output Format in two columns (Row 3)
    auto *row3 = new QHBoxLayout();
    row3->setSpacing(12);

    auto *langCol = new QVBoxLayout();
    langCol->setSpacing(4);
    langCol->addWidget(makeLabel(Locale::tr(QStringLiteral("office.dialog.language"))));
    m_langCombo = new QComboBox(this);
    for (const auto &l : Languages) {
        m_langCombo->addItem(QString::fromUtf8(l.name), QString::fromLatin1(l.tag));
    }
    const QString curLang = Locale::language();
    for (int i = 0; i < m_langCombo->count(); ++i) {
        if (m_langCombo->itemData(i).toString().startsWith(curLang, Qt::CaseInsensitive)) {
            m_langCombo->setCurrentIndex(i);
            break;
        }
    }
    langCol->addWidget(m_langCombo);
    row3->addLayout(langCol, 1);

    auto *outCol = new QVBoxLayout();
    outCol->setSpacing(4);
    outCol->addWidget(makeLabel(Locale::tr(QStringLiteral("office.yaoctru.output_format"))));
    m_outputCombo = new QComboBox(this);
    m_outputCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.format_aria2")), QStringLiteral("aria2"));
    m_outputCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.format_curl")), QStringLiteral("curl"));
    m_outputCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.format_wget")), QStringLiteral("wget"));
    m_outputCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.format_text")), QStringLiteral("text"));
    outCol->addWidget(m_outputCombo);
    row3->addLayout(outCol, 1);

    mainLayout->addLayout(row3);

    // 4. Output Path
    auto *pathCol = new QVBoxLayout();
    pathCol->setSpacing(4);
    pathCol->addWidget(makeLabel(Locale::tr(QStringLiteral("office.dialog.path"))));
    auto *pathRow = new QHBoxLayout();
    pathRow->setSpacing(8);

    m_pathEdit = new QLineEdit(this);
    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QStringLiteral("/Office");
    m_pathEdit->setText(QDir::toNativeSeparators(defaultPath));
    connect(m_pathEdit, &QLineEdit::textChanged, this, &YaoctruDialog::updateFreeSpace);
    pathRow->addWidget(m_pathEdit, 1);

    m_browseBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.dialog.browse")), this);
    connect(m_browseBtn, &PillButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, Locale::tr(QStringLiteral("office.dialog.path")), m_pathEdit->text());
        if (!dir.isEmpty()) {
            m_pathEdit->setText(QDir::toNativeSeparators(dir));
        }
    });
    pathRow->addWidget(m_browseBtn);
    pathCol->addLayout(pathRow);

    // Boş alan etiketi
    m_freeSpaceLabel = new QLabel(this);
    m_freeSpaceLabel->setFont(Theme::sans(10.5, Theme::Weight::Regular));
    pathCol->addWidget(m_freeSpaceLabel);

    mainLayout->addLayout(pathCol);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setFont(Theme::sans(10.5, Theme::Weight::Medium));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #0284c7;"));
    m_statusLabel->hide();
    mainLayout->addWidget(m_statusLabel);

    // 5. Result / Preview Box (initially hidden)
    m_resultWidget = new QWidget(this);
    auto *resLayout = new QVBoxLayout(m_resultWidget);
    resLayout->setContentsMargins(0, 2, 0, 0);
    resLayout->setSpacing(6);

    auto *resHeaderLayout = new QHBoxLayout();
    m_resultFileLabel = new QLabel(this);
    m_resultFileLabel->setFont(Theme::sans(10.5, Theme::Weight::SemiBold));
    m_resultFileLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::accent().name(QColor::HexRgb)));
    resHeaderLayout->addWidget(m_resultFileLabel, 1);

    m_copyBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.yaoctru.copy")), this);
    connect(m_copyBtn, &PillButton::clicked, this, &YaoctruDialog::onCopyClicked);
    resHeaderLayout->addWidget(m_copyBtn);

    m_saveBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.yaoctru.save")), this);
    connect(m_saveBtn, &PillButton::clicked, this, &YaoctruDialog::onSaveClicked);
    resHeaderLayout->addWidget(m_saveBtn);

    m_runBtn = new PillButton(PillButton::Accent, Locale::tr(QStringLiteral("office.yaoctru.run")), this);
    connect(m_runBtn, &PillButton::clicked, this, &YaoctruDialog::onRunClicked);
    resHeaderLayout->addWidget(m_runBtn);

    resLayout->addLayout(resHeaderLayout);

    m_previewEdit = new QPlainTextEdit(this);
    m_previewEdit->setFixedHeight(85);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setFont(Theme::mono(9.5));
    m_previewEdit->setStyleSheet(QStringLiteral("background: rgba(0,0,0,0.15); border: 1px solid rgba(120,120,120,0.25); border-radius: 6px; padding: 4px;"));
    resLayout->addWidget(m_previewEdit);

    m_resultWidget->hide();
    mainLayout->addWidget(m_resultWidget);

    // 6. Dialog Bottom Buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    m_cancelBtn = new PillButton(PillButton::Ghost, Locale::tr(QStringLiteral("office.dialog.cancel")), this);
    connect(m_cancelBtn, &PillButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_cancelBtn);

    m_generateBtn = new PillButton(PillButton::Accent, Locale::tr(QStringLiteral("office.yaoctru.generate")), this);
    connect(m_generateBtn, &PillButton::clicked, this, &YaoctruDialog::onGenerateClicked);
    btnRow->addWidget(m_generateBtn);

    mainLayout->addLayout(btnRow);
}

void YaoctruDialog::updateFreeSpace()
{
    QString checkPath = m_pathEdit->text().trimmed();
    while (!checkPath.isEmpty() && !QDir(checkPath).exists()) {
        const QString parent = QFileInfo(checkPath).dir().absolutePath();
        if (parent == checkPath)
            break;
        checkPath = parent;
    }
    QStorageInfo storage(checkPath);
    if (storage.isValid() && m_freeSpaceLabel) {
        const double freeGb = double(storage.bytesAvailable()) / (1024.0 * 1024.0 * 1024.0);
        m_freeSpaceLabel->setText(Locale::tr(QStringLiteral("office.dialog.free_space")).arg(freeGb, 0, 'f', 1));
        m_freeSpaceLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Color::TextSecondary().name(QColor::HexRgb)));
    } else if (m_freeSpaceLabel) {
        m_freeSpaceLabel->setText(QString());
    }
}

void YaoctruDialog::updateArchCombo()
{
    const QString curArch = m_archCombo->currentData().toString();
    const QString osLevel = m_osCombo->currentData().toString();

    m_archCombo->blockSignals(true);
    m_archCombo->clear();

    m_archCombo->addItem(QStringLiteral("x64 (64-Bit)"), QStringLiteral("x64"));
    m_archCombo->addItem(QStringLiteral("x86 (32-Bit)"), QStringLiteral("x86"));
    m_archCombo->addItem(QStringLiteral("x86x64 (Multi)"), QStringLiteral("x86x64"));

    // ARM is only valid on Windows 10/11
    if (osLevel.isEmpty()) {
        m_archCombo->addItem(QStringLiteral("x64arm64 (ARM64 Native)"), QStringLiteral("x64arm64"));
        m_archCombo->addItem(QStringLiteral("x86arm64 (ARM64 Emulated)"), QStringLiteral("x86arm64"));
    }

    int idx = m_archCombo->findData(curArch);
    if (idx < 0) idx = 0;
    m_archCombo->setCurrentIndex(idx);
    m_archCombo->blockSignals(false);
}

void YaoctruDialog::updateChannelCombo()
{
    const QString osLevel = m_osCombo->currentData().toString();
    const QString curChannelId = m_channelCombo->currentData().toString();

    m_channelCombo->blockSignals(true);
    m_channelCombo->clear();

    for (const auto &c : Channels) {
        if (osLevel == QLatin1String("Win7") && !c.supportsWin7)
            continue;
        if (osLevel == QLatin1String("Win81") && !c.supportsWin81)
            continue;
        m_channelCombo->addItem(QString::fromUtf8(c.name), QString::fromLatin1(c.id));
    }

    int idx = m_channelCombo->findData(curChannelId);
    if (idx < 0) idx = 0;
    m_channelCombo->setCurrentIndex(idx);
    m_channelCombo->blockSignals(false);
}

void YaoctruDialog::updateOsCombo()
{
    const QString channelId = m_channelCombo->currentData().toString();
    const QString curOsLevel = m_osCombo->currentData().toString();

    const ChannelEntry *entry = nullptr;
    for (const auto &c : Channels) {
        if (QString::fromLatin1(c.id) == channelId) {
            entry = &c;
            break;
        }
    }

    m_osCombo->blockSignals(true);
    m_osCombo->clear();

    m_osCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.os_default")), QString());

    if (entry && entry->supportsWin81) {
        m_osCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.os_win81")), QStringLiteral("Win81"));
    }
    if (entry && entry->supportsWin7) {
        m_osCombo->addItem(Locale::tr(QStringLiteral("office.yaoctru.os_win7")), QStringLiteral("Win7"));
    }

    int idx = m_osCombo->findData(curOsLevel);
    if (idx < 0) idx = 0;
    m_osCombo->setCurrentIndex(idx);
    m_osCombo->blockSignals(false);

    updateArchCombo();
}

void YaoctruDialog::onOsLevelChanged(int)
{
    updateChannelCombo();
    updateArchCombo();
}

void YaoctruDialog::onChannelChanged(int)
{
    updateOsCombo();
}

void YaoctruDialog::onGenerateClicked()
{
    m_generateBtn->setEnabled(false);
    m_statusLabel->setText(Locale::tr(QStringLiteral("office.yaoctru.generating")));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #0284c7;"));
    m_statusLabel->show();

    const QString channelGuid = m_channelCombo->currentData().toString();
    const QString osLevel = m_osCombo->currentData().toString();

    QString apiUrl = QStringLiteral("https://mrodevicemgr.officeapps.live.com/mrodevicemgrsvc/api/v2/C2RReleaseData?audienceFFN=") + channelGuid;
    if (osLevel == QLatin1String("Win7")) {
        apiUrl += QStringLiteral("&osver=Client|6.1");
    } else if (osLevel == QLatin1String("Win81")) {
        apiUrl += QStringLiteral("&osver=Client|6.3");
    }

    QNetworkRequest req((QUrl(apiUrl)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "Arbitrium/1.0 (Windows NT; x64)");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, osLevel]() {
        reply->deleteLater();
        m_generateBtn->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("Hata: %1").arg(reply->errorString()));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #ef4444;"));
            return;
        }

        const QByteArray replyBytes = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(replyBytes);
        const QString type = doc.object().value(QStringLiteral("Type")).toString();
        const QString build = doc.object().value(QStringLiteral("AvailableBuild")).toString();

        if (build.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("Hata: Sürüm bilgisi alınamadı."));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #ef4444;"));
            return;
        }

        if (osLevel == QLatin1String("Win7") && type != QLatin1String("Custom Win7")) {
            m_statusLabel->setText(QStringLiteral("Hata: Bu kanal Windows 7 için sürüm sunmuyor."));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #ef4444;"));
            return;
        }
        if (osLevel == QLatin1String("Win81") && type != QLatin1String("Custom Win8.1")) {
            m_statusLabel->setText(QStringLiteral("Hata: Bu kanal Windows 8.1 için sürüm sunmuyor."));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #ef4444;"));
            return;
        }

        m_statusLabel->hide();
        generateScript(build);
    });
}

void YaoctruDialog::generateScript(const QString &build)
{
    const int chIdx = m_channelCombo->currentIndex();
    const QString channelGuid = m_channelCombo->currentData().toString();
    const QString channelCmdTag = (chIdx >= 0 && chIdx < 8) ? QString::fromLatin1(Channels[chIdx].cmdTag) : QStringLiteral("Office");

    const QString osLevel = m_osCombo->currentData().toString();
    const QString osSuffix = (osLevel == QLatin1String("Win7")) ? QStringLiteral("_W7") :
                             (osLevel == QLatin1String("Win81")) ? QStringLiteral("_W81") : QString();

    const QString bitness = m_archCombo->currentData().toString();
    const int langIdx = m_langCombo->currentIndex();
    const QString langTag = (langIdx >= 0) ? QString::fromLatin1(Languages[langIdx].tag) : QStringLiteral("en-us");
    const int lcid = (langIdx >= 0) ? Languages[langIdx].lcid : 1033;
    const QString prodType = m_productCombo->currentData().toString();
    const QString outFormat = m_outputCombo->currentData().toString();

    QString outArch, outBit;
    const QStringList urls = generateUrls(channelGuid, build, langTag, lcid, bitness, prodType, outArch, outBit);

    QString prodTag;
    if (prodType == QLatin1String("Lang")) prodTag = QStringLiteral("_LangPack");
    else if (prodType == QLatin1String("Proof")) prodTag = QStringLiteral("_Proofing");

    m_generatedFileName = QStringLiteral("%1_%2_%3%4_%5%6")
                              .arg(build, bitness, langTag, prodTag, channelCmdTag, osSuffix);

    const QString prodDesc = (prodType == QLatin1String("Lang")) ? QStringLiteral("Language Pack") :
                             (prodType == QLatin1String("Proof")) ? QStringLiteral("Proofing Tools") :
                             QStringLiteral("Full Office Source");

    const QString osDesc = (osLevel == QLatin1String("Win7")) ? QStringLiteral("Windows 7 (SP1)") :
                           (osLevel == QLatin1String("Win81")) ? QStringLiteral("Windows 8.1") :
                           QStringLiteral("Windows 10 / 11");

    if (outFormat == QLatin1String("aria2")) {
        m_generatedContent = generateAria2Script(urls, channelCmdTag, build, outArch, langTag, prodDesc, osDesc);
        m_generatedFileName += QStringLiteral("_aria2.bat");
        m_runBtn->show();
    } else if (outFormat == QLatin1String("curl")) {
        m_generatedContent = generateCurlScript(urls, channelCmdTag, build, outArch, langTag, prodDesc, osDesc);
        m_generatedFileName += QStringLiteral("_curl.bat");
        m_runBtn->show();
    } else if (outFormat == QLatin1String("wget")) {
        m_generatedContent = generateWgetScript(urls, channelCmdTag, build, outArch, langTag, prodDesc, osDesc);
        m_generatedFileName += QStringLiteral("_wget.bat");
        m_runBtn->show();
    } else {
        m_generatedContent = urls.join(QStringLiteral("\r\n"));
        m_generatedFileName += QStringLiteral(".txt");
        m_runBtn->hide();
    }

    m_resultFileLabel->setText(QStringLiteral("📄 %1 (%2 Bağlantı)").arg(m_generatedFileName).arg(urls.size()));
    m_previewEdit->setPlainText(m_generatedContent);
    m_resultWidget->show();
    m_savedFilePath.clear();
    if (parentWidget()) {
        const QRect parentRect = parentWidget()->geometry();
        const int x = parentRect.x() + (parentRect.width() - width()) / 2;
        int y = parentRect.y() + qMax(16, (parentRect.height() - height()) / 2);
        if (y + height() > parentRect.bottom() - 12) {
            y = qMax(parentRect.top() + 16, parentRect.bottom() - height() - 12);
        }
        move(x, y);
    }
}

QStringList YaoctruDialog::generateUrls(const QString &channelGuid, const QString &version,
                                      const QString &lang, int lcid, const QString &bitness,
                                      const QString &productType, QString &outArch, QString &outBit)
{
    const QString baseUrl = QStringLiteral("https://officecdn.microsoft.com/db/%1/Office/Data").arg(channelGuid);

    const bool isDual = (bitness == QLatin1String("x86x64"));
    const bool isChpe = (bitness == QLatin1String("x86arm64"));
    const bool isXarm = (bitness == QLatin1String("x64arm64"));

    outArch = bitness;
    if (isDual || isChpe) outArch = QStringLiteral("x86");
    if (isXarm) outArch = QStringLiteral("x64");
    outBit = (outArch == QLatin1String("x64")) ? QStringLiteral("64") : QStringLiteral("32");

    const QString verUrl = baseUrl + QLatin1Char('/') + version;
    const QString lcidStr = QString::number(lcid);

    QStringList urls;
    urls << baseUrl + QStringLiteral("/v") + outBit + QStringLiteral(".cab");
    urls << baseUrl + QStringLiteral("/v") + outBit + QLatin1Char('_') + version + QStringLiteral(".cab");

    if (productType == QLatin1String("Proof")) {
        urls << verUrl + QStringLiteral("/sp") + outBit + lcidStr + QStringLiteral(".cab");
        urls << verUrl + QStringLiteral("/i") + outBit + QStringLiteral("0.cab");
        urls << verUrl + QStringLiteral("/s") + outBit + QStringLiteral("0.cab");
        urls << verUrl + QStringLiteral("/stream.") + outArch + QLatin1Char('.') + lang + QStringLiteral(".proof.dat");

        if (outArch == QLatin1String("x86"))
            urls << verUrl + QStringLiteral("/i640.cab");

        if (isDual) {
            urls << baseUrl + QStringLiteral("/v64.cab");
            urls << baseUrl + QStringLiteral("/v64_") + version + QStringLiteral(".cab");
            urls << verUrl + QStringLiteral("/sp64") + lcidStr + QStringLiteral(".cab");
            urls << verUrl + QStringLiteral("/i640.cab");
            urls << verUrl + QStringLiteral("/s640.cab");
            urls << verUrl + QStringLiteral("/stream.x64.") + lang + QStringLiteral(".proof.dat");
        }
    } else {
        urls << verUrl + QStringLiteral("/i") + outBit + lcidStr + QStringLiteral(".cab");
        urls << verUrl + QStringLiteral("/s") + outBit + lcidStr + QStringLiteral(".cab");
        urls << verUrl + QStringLiteral("/i") + outBit + QStringLiteral("0.cab");
        urls << verUrl + QStringLiteral("/s") + outBit + QStringLiteral("0.cab");
        urls << verUrl + QStringLiteral("/stream.") + outArch + QLatin1Char('.') + lang + QStringLiteral(".dat");

        if (isChpe) {
            urls << verUrl + QStringLiteral("/sc320.cab");
            urls << verUrl + QStringLiteral("/stream.x86.x-none.chpe.dat");
        }
        if (isXarm) {
            urls << verUrl + QStringLiteral("/sa640.cab");
            urls << verUrl + QStringLiteral("/stream.x64.x-none.arm64x.dat");
        }
        if (outArch == QLatin1String("x86") && !isChpe && !isDual) {
            urls << verUrl + QStringLiteral("/i64") + lcidStr + QStringLiteral(".cab");
            urls << verUrl + QStringLiteral("/i640.cab");
        }
        if (productType == QLatin1String("Full")) {
            urls << verUrl + QStringLiteral("/stream.") + outArch + QStringLiteral(".x-none.dat");
        }
        if (productType == QLatin1String("Lang")) {
            urls << QStringLiteral("https://officecdn.microsoft.com/db/wsus/SetupLanguagePack.") + outArch + QLatin1Char('.') + lang + QStringLiteral(".exe");
        }
        if (isDual) {
            urls << baseUrl + QStringLiteral("/v64.cab");
            urls << baseUrl + QStringLiteral("/v64_") + version + QStringLiteral(".cab");
            urls << verUrl + QStringLiteral("/i64") + lcidStr + QStringLiteral(".cab");
            urls << verUrl + QStringLiteral("/s64") + lcidStr + QStringLiteral(".cab");
            urls << verUrl + QStringLiteral("/i640.cab");
            urls << verUrl + QStringLiteral("/s640.cab");
            urls << verUrl + QStringLiteral("/stream.x64.") + lang + QStringLiteral(".dat");
            if (productType == QLatin1String("Full")) {
                urls << verUrl + QStringLiteral("/stream.x64.x-none.dat");
            }
            if (productType == QLatin1String("Lang")) {
                urls << QStringLiteral("https://officecdn.microsoft.com/db/wsus/SetupLanguagePack.x64.") + lang + QStringLiteral(".exe");
            }
        }
    }
    return urls;
}

QString YaoctruDialog::generateAria2Script(const QStringList &urls, const QString &channelName,
                                          const QString &version, const QString &arch, const QString &lang,
                                          const QString &prodType, const QString &osLevel)
{
    QString s;
    QTextStream ts(&s);
    ts << "@echo off\r\n";
    ts << ":: Limit download speed (e.g. 1M, 500K, 0 = unlimited)\r\n";
    ts << "set \"speedLimit=0\"\r\n";
    ts << "set \"parallel=1\"\r\n";
    ts << "set \"_work=%~dp0\"\r\n";
    ts << "set \"_work=%_work:~0,-1%\"\r\n";
    ts << "setlocal EnableDelayedExpansion\r\n";
    ts << "pushd \"!_work!\"\r\n\r\n";
    ts << ":: Auto-detect aria2c.exe (Arbitrium temp folder, current directory, or PATH)\r\n";
    ts << "set \"ARIA2=\"\r\n";
    ts << "if exist \"%TEMP%\\Arbitrium\\aria2c.exe\" set \"ARIA2=%TEMP%\\Arbitrium\\aria2c.exe\"\r\n";
    ts << "if exist \"%~dp0aria2c.exe\" set \"ARIA2=%~dp0aria2c.exe\"\r\n";
    ts << "if exist \"aria2c.exe\" set \"ARIA2=aria2c.exe\"\r\n";
    ts << "if not defined ARIA2 (\r\n";
    ts << "    for %%i in (aria2c.exe) do if not \"%%~$PATH:i\"==\"\" set \"ARIA2=aria2c.exe\"\r\n";
    ts << ")\r\n";
    ts << "if not defined ARIA2 (\r\n";
    ts << "    echo.\r\n";
    ts << "    echo Error: aria2c.exe was not detected.\r\n";
    ts << "    echo Please place aria2c.exe in this folder or run Arbitrium once.\r\n";
    ts << "    echo.\r\n";
    ts << "    popd\r\n";
    ts << "    pause\r\n";
    ts << "    exit /b 1\r\n";
    ts << ")\r\n\r\n";
    ts << "set \"destDir=C2R_" << channelName << "\"\r\n";
    ts << "set \"uri=temp_aria2.txt\"\r\n\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Office Click-to-Run Downloader (Aria2 Engine)\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Channel       : " << channelName << "\r\n";
    ts << "echo  Version       : " << version << "\r\n";
    ts << "echo  Architecture  : " << arch << "\r\n";
    ts << "echo  Language      : " << lang << "\r\n";
    ts << "echo  Product Type  : " << prodType << "\r\n";
    ts << "echo  Target OS     : " << osLevel << "\r\n";
    ts << "echo  Total Files   : " << QString::number(urls.size()) << "\r\n";
    ts << "echo  Destination   : %destDir%\\\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo.\r\n";
    ts << "echo [1/2] Creating temporary download queue list...\r\n";
    ts << "if exist \"%uri%\" del /f /q \"%uri%\"\r\n";
    ts << "(\r\n";
    for (const QString &u : urls) {
        const QString fname = QFileInfo(u).fileName();
        QString relPath = fname;
        if (u.contains(QStringLiteral("/Data/v"))) {
            relPath = QStringLiteral("Office\\Data\\") + fname;
        } else if (u.contains(QStringLiteral("/Data/"))) {
            relPath = QStringLiteral("Office\\Data\\") + version + QStringLiteral("\\") + fname;
        }
        ts << "echo " << u << "\r\n";
        ts << "echo   out=" << relPath << "\r\n";
        ts << "echo.\r\n";
    }
    ts << ") > \"%uri%\"\r\n\r\n";
    ts << "echo [2/2] Starting 16-connection parallel download with Aria2...\r\n";
    ts << "echo.\r\n";
    ts << "\"%ARIA2%\" -x16 -s16 -j%parallel% -c -R --file-allocation=none --max-overall-download-limit=%speedLimit% -d\"%destDir%\" -i\"%uri%\"\r\n";
    ts << "if exist \"%uri%\" del /f /q \"%uri%\"\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\v32_*.cab\" copy /y \"%destDir%\\Office\\Data\\v32_*.cab\" \"%destDir%\\Office\\Data\\v32.cab\" >nul\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\v64_*.cab\" copy /y \"%destDir%\\Office\\Data\\v64_*.cab\" \"%destDir%\\Office\\Data\\v64.cab\" >nul\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\SetupLanguagePack*.exe\" move /y \"%destDir%\\Office\\Data\\SetupLanguagePack*.exe\" \"%destDir%\\\" >nul\r\n";
    ts << "if exist \"%~dp0start_setup.cmd\" copy /y \"%~dp0start_setup.cmd\" \"%destDir%\\start_setup.cmd\" >nul\r\n";
    ts << "echo.\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Download completed successfully!\r\n";
    ts << "echo  Files saved to: %destDir%\\\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "popd\r\n";
    ts << "pause\r\n";
    return s;
}

QString YaoctruDialog::generateCurlScript(const QStringList &urls, const QString &channelName,
                                         const QString &version, const QString &arch, const QString &lang,
                                         const QString &prodType, const QString &osLevel)
{
    QString s;
    QTextStream ts(&s);
    ts << "@echo off\r\n";
    ts << ":: Limit download speed (e.g. 1M, 500K, empty = unlimited)\r\n";
    ts << "set \"speedLimit=\"\r\n";
    ts << "if defined speedLimit set \"speedLimit=--limit-rate %speedLimit%\"\r\n";
    ts << "set \"_work=%~dp0\"\r\n";
    ts << "set \"_work=%_work:~0,-1%\"\r\n";
    ts << "setlocal EnableDelayedExpansion\r\n";
    ts << "pushd \"!_work!\"\r\n\r\n";
    ts << ":: Auto-detect curl.exe (Arbitrium temp folder, current directory, System32, or PATH)\r\n";
    ts << "set \"CURL=\"\r\n";
    ts << "if exist \"%TEMP%\\Arbitrium\\curl.exe\" set \"CURL=%TEMP%\\Arbitrium\\curl.exe\"\r\n";
    ts << "if exist \"%~dp0curl.exe\" set \"CURL=%~dp0curl.exe\"\r\n";
    ts << "if exist \"curl.exe\" set \"CURL=curl.exe\"\r\n";
    ts << "if exist \"%SystemRoot%\\System32\\curl.exe\" set \"CURL=%SystemRoot%\\System32\\curl.exe\"\r\n";
    ts << "if not defined CURL (\r\n";
    ts << "    for %%i in (curl.exe) do if not \"%%~$PATH:i\"==\"\" set \"CURL=curl.exe\"\r\n";
    ts << ")\r\n";
    ts << "if not defined CURL (\r\n";
    ts << "    echo.\r\n";
    ts << "    echo Error: curl.exe was not detected.\r\n";
    ts << "    echo Please place curl.exe in this folder or run Arbitrium once.\r\n";
    ts << "    echo.\r\n";
    ts << "    popd\r\n";
    ts << "    pause\r\n";
    ts << "    exit /b 1\r\n";
    ts << ")\r\n\r\n";
    ts << "set \"destDir=C2R_" << channelName << "\"\r\n\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Office Click-to-Run Downloader (cURL Engine)\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Channel       : " << channelName << "\r\n";
    ts << "echo  Version       : " << version << "\r\n";
    ts << "echo  Architecture  : " << arch << "\r\n";
    ts << "echo  Language      : " << lang << "\r\n";
    ts << "echo  Product Type  : " << prodType << "\r\n";
    ts << "echo  Target OS     : " << osLevel << "\r\n";
    ts << "echo  Total Files   : " << QString::number(urls.size()) << "\r\n";
    ts << "echo  Destination   : %destDir%\\\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo.\r\n";
    int fileIdx = 0;
    const int totalFiles = urls.size();
    for (const QString &u : urls) {
        fileIdx++;
        const QString fname = QFileInfo(u).fileName();
        QString relPath;
        if (u.contains(QStringLiteral("/Data/v"))) {
            relPath = QStringLiteral("%destDir%\\Office\\Data\\") + fname;
        } else if (u.contains(QStringLiteral("/Data/"))) {
            relPath = QStringLiteral("%destDir%\\Office\\Data\\") + version + QStringLiteral("\\") + fname;
        } else {
            relPath = QStringLiteral("%destDir%\\") + fname;
        }
        ts << "echo [" << QString::number(fileIdx) << "/" << QString::number(totalFiles) << "] Downloading " << fname << "...\r\n";
        ts << "\"%CURL%\" -q --create-dirs --retry 5 %speedLimit% -k -L -C - -o \"" << relPath << "\" \"" << u << "\"\r\n";
        ts << "if errorlevel 1 echo Warning: Failed to download " << fname << "\r\n\r\n";
    }
    ts << "if exist \"%destDir%\\Office\\Data\\v32_*.cab\" copy /y \"%destDir%\\Office\\Data\\v32_*.cab\" \"%destDir%\\Office\\Data\\v32.cab\" >nul\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\v64_*.cab\" copy /y \"%destDir%\\Office\\Data\\v64_*.cab\" \"%destDir%\\Office\\Data\\v64.cab\" >nul\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\SetupLanguagePack*.exe\" move /y \"%destDir%\\Office\\Data\\SetupLanguagePack*.exe\" \"%destDir%\\\" >nul\r\n";
    ts << "if exist \"%~dp0start_setup.cmd\" copy /y \"%~dp0start_setup.cmd\" \"%destDir%\\start_setup.cmd\" >nul\r\n";
    ts << "echo.\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Download completed successfully!\r\n";
    ts << "echo  Files saved to: %destDir%\\\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "popd\r\n";
    ts << "pause\r\n";
    return s;
}

QString YaoctruDialog::generateWgetScript(const QStringList &urls, const QString &channelName,
                                         const QString &version, const QString &arch, const QString &lang,
                                         const QString &prodType, const QString &osLevel)
{
    QString s;
    QTextStream ts(&s);
    ts << "@echo off\r\n";
    ts << "set \"speedLimit=0\"\r\n";
    ts << "set \"_work=%~dp0\"\r\n";
    ts << "set \"_work=%_work:~0,-1%\"\r\n";
    ts << "setlocal EnableDelayedExpansion\r\n";
    ts << "pushd \"!_work!\"\r\n\r\n";
    ts << ":: Auto-detect wget.exe (Arbitrium temp folder, current directory, or PATH)\r\n";
    ts << "set \"WGET=\"\r\n";
    ts << "if exist \"%TEMP%\\Arbitrium\\wget.exe\" set \"WGET=%TEMP%\\Arbitrium\\wget.exe\"\r\n";
    ts << "if exist \"%~dp0wget.exe\" set \"WGET=%~dp0wget.exe\"\r\n";
    ts << "if exist \"wget.exe\" set \"WGET=wget.exe\"\r\n";
    ts << "if not defined WGET (\r\n";
    ts << "    for %%i in (wget.exe) do if not \"%%~$PATH:i\"==\"\" set \"WGET=wget.exe\"\r\n";
    ts << ")\r\n";
    ts << "if not defined WGET (\r\n";
    ts << "    echo.\r\n";
    ts << "    echo Error: wget.exe was not detected.\r\n";
    ts << "    echo Please place wget.exe in this folder or run Arbitrium once.\r\n";
    ts << "    echo.\r\n";
    ts << "    popd\r\n";
    ts << "    pause\r\n";
    ts << "    exit /b 1\r\n";
    ts << ")\r\n\r\n";
    ts << "set \"destDir=C2R_" << channelName << "\"\r\n";
    ts << "set \"uri=temp_wget.txt\"\r\n\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Office Click-to-Run Downloader (Wget Engine)\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Channel       : " << channelName << "\r\n";
    ts << "echo  Version       : " << version << "\r\n";
    ts << "echo  Architecture  : " << arch << "\r\n";
    ts << "echo  Language      : " << lang << "\r\n";
    ts << "echo  Product Type  : " << prodType << "\r\n";
    ts << "echo  Target OS     : " << osLevel << "\r\n";
    ts << "echo  Total Files   : " << QString::number(urls.size()) << "\r\n";
    ts << "echo  Destination   : %destDir%\\\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo.\r\n";
    ts << "echo [1/2] Creating temporary download queue list...\r\n";
    ts << "if exist \"%uri%\" del /f /q \"%uri%\"\r\n";
    ts << "(\r\n";
    for (const QString &u : urls) {
        ts << "echo " << u << "\r\n";
    }
    ts << ") > \"%uri%\"\r\n\r\n";
    ts << "echo [2/2] Starting download with Wget...\r\n";
    ts << "echo.\r\n";
    ts << "\"%WGET%\" --limit-rate=%speedLimit% --directory-prefix=\"%destDir%\" --input-file=\"%uri%\" --no-verbose --show-progress --continue --retry-connrefused --tries=5 --ignore-case --force-directories --no-host-directories --cut-dirs=2\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\v32_*.cab\" copy /y \"%destDir%\\Office\\Data\\v32_*.cab\" \"%destDir%\\Office\\Data\\v32.cab\" >nul\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\v64_*.cab\" copy /y \"%destDir%\\Office\\Data\\v64_*.cab\" \"%destDir%\\Office\\Data\\v64.cab\" >nul\r\n";
    ts << "if exist \"%destDir%\\Office\\Data\\SetupLanguagePack*.exe\" move /y \"%destDir%\\Office\\Data\\SetupLanguagePack*.exe\" \"%destDir%\\\" >nul\r\n";
    ts << "if exist \"%~dp0start_setup.cmd\" copy /y \"%~dp0start_setup.cmd\" \"%destDir%\\start_setup.cmd\" >nul\r\n";
    ts << "if exist \"%uri%\" del /f /q \"%uri%\"\r\n";
    ts << "echo.\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "echo  Download completed successfully!\r\n";
    ts << "echo  Files saved to: %destDir%\\\r\n";
    ts << "echo ======================================================================\r\n";
    ts << "popd\r\n";
    ts << "pause\r\n";
    return s;
}

void YaoctruDialog::onCopyClicked()
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb && !m_generatedContent.isEmpty()) {
        cb->setText(m_generatedContent);
        m_copyBtn->setText(Locale::tr(QStringLiteral("office.yaoctru.copied")));
    }
}

void YaoctruDialog::onSaveClicked()
{
    const QString dir = m_pathEdit->text().trimmed();
    QDir().mkpath(dir);
    const QString targetPath = QDir(dir).filePath(m_generatedFileName);

    QFile f(targetPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << m_generatedContent;
        f.close();
        m_savedFilePath = targetPath;
        m_saveBtn->setText(Locale::tr(QStringLiteral("office.yaoctru.saved")));
    }

    const QString outFormat = m_outputCombo->currentData().toString();
    if (outFormat == QLatin1String("aria2")) {
        const QString ariaSrc = ensureAria2Extracted();
        const QString ariaDst = QDir(dir).filePath(QStringLiteral("aria2c.exe"));
        if (QFile::exists(ariaSrc) && (!QFile::exists(ariaDst) || QFileInfo(ariaDst).size() != QFileInfo(ariaSrc).size())) {
            QFile::remove(ariaDst);
            QFile::copy(ariaSrc, ariaDst);
        }
    } else if (outFormat == QLatin1String("wget")) {
        const QString wgetSrc = ensureWgetExtracted();
        const QString wgetDst = QDir(dir).filePath(QStringLiteral("wget.exe"));
        if (QFile::exists(wgetSrc) && (!QFile::exists(wgetDst) || QFileInfo(wgetDst).size() != QFileInfo(wgetSrc).size())) {
            QFile::remove(wgetDst);
            QFile::copy(wgetSrc, wgetDst);
        }
    } else if (outFormat == QLatin1String("curl")) {
        const QString curlSrc = ensureCurlExtracted();
        const QString curlDst = QDir(dir).filePath(QStringLiteral("curl.exe"));
        if (!curlSrc.isEmpty() && QFile::exists(curlSrc) && (!QFile::exists(curlDst) || QFileInfo(curlDst).size() != QFileInfo(curlSrc).size())) {
            QFile::remove(curlDst);
            QFile::copy(curlSrc, curlDst);
        }
    }

    // İndirilen paketin tek tıkla kurulabilmesi için start_setup.cmd kopyala
    QFile scriptRes(QStringLiteral(":/scripts/start_setup_c2r.cmd"));
    const QString targetScript = QDir(dir).filePath(QStringLiteral("start_setup.cmd"));
    if (scriptRes.open(QIODevice::ReadOnly)) {
        QFile outScript(targetScript);
        if (outScript.open(QIODevice::WriteOnly)) {
            outScript.write(scriptRes.readAll());
            outScript.close();
        }
        scriptRes.close();
    }
}

void YaoctruDialog::onRunClicked()
{
    if (m_savedFilePath.isEmpty() || !QFile::exists(m_savedFilePath)) {
        onSaveClicked();
    }

    const QString outFormat = m_outputCombo->currentData().toString();
    if (outFormat == QLatin1String("aria2")) {
        ensureAria2Extracted();
    } else if (outFormat == QLatin1String("wget")) {
        ensureWgetExtracted();
    } else if (outFormat == QLatin1String("curl")) {
        ensureCurlExtracted();
    }

    if (!m_savedFilePath.isEmpty() && QFile::exists(m_savedFilePath)) {
        const QString workDir = QFileInfo(m_savedFilePath).dir().absolutePath();
        QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("start"), QStringLiteral(""), m_savedFilePath}, workDir);
    }
}

void YaoctruDialog::retranslate()
{
    using namespace Theme;

    m_titleLabel->setText(Locale::tr(QStringLiteral("office.yaoctru.dialog_title")));
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Color::TextPrimary().name(QColor::HexRgb)));

    m_descLabel->setText(Locale::tr(QStringLiteral("office.card.yaoctru.desc")));
    m_descLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Color::TextSecondary().name(QColor::HexRgb)));

    const QString comboStyle = QStringLiteral(
        "QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px 12px; }"
        "QComboBox:focus { border: 1.5px solid %4; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { background: %1; color: %2; selection-background-color: %4; }")
        .arg(Color::Surface().name(QColor::HexRgb), Color::TextPrimary().name(QColor::HexRgb),
             Color::BorderControl().name(QColor::HexRgb), Theme::accent().name(QColor::HexRgb));

    m_osCombo->setStyleSheet(comboStyle);
    m_channelCombo->setStyleSheet(comboStyle);
    m_archCombo->setStyleSheet(comboStyle);
    m_productCombo->setStyleSheet(comboStyle);
    m_langCombo->setStyleSheet(comboStyle);
    m_outputCombo->setStyleSheet(comboStyle);

    m_pathEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px 10px; }"
        "QLineEdit:focus { border: 1.5px solid %4; }")
        .arg(Color::Surface().name(QColor::HexRgb), Color::TextPrimary().name(QColor::HexRgb),
             Color::BorderControl().name(QColor::HexRgb), Theme::accent().name(QColor::HexRgb)));

    m_browseBtn->setText(Locale::tr(QStringLiteral("office.dialog.browse")));
    m_cancelBtn->setText(Locale::tr(QStringLiteral("office.dialog.cancel")));
    m_generateBtn->setText(Locale::tr(QStringLiteral("office.yaoctru.generate")));
    m_copyBtn->setText(Locale::tr(QStringLiteral("office.yaoctru.copy")));
    m_saveBtn->setText(Locale::tr(QStringLiteral("office.yaoctru.save")));
    m_runBtn->setText(Locale::tr(QStringLiteral("office.yaoctru.run")));

    // ComboBox metinlerini aktif dili koruyarak güncelle
    m_osCombo->blockSignals(true);
    for (int i = 0; i < m_osCombo->count(); ++i) {
        const QString d = m_osCombo->itemData(i).toString();
        if (d.isEmpty())
            m_osCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.os_default")));
        else if (d == QLatin1String("Win81"))
            m_osCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.os_win81")));
        else if (d == QLatin1String("Win7"))
            m_osCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.os_win7")));
    }
    m_osCombo->blockSignals(false);

    m_productCombo->blockSignals(true);
    for (int i = 0; i < m_productCombo->count(); ++i) {
        const QString d = m_productCombo->itemData(i).toString();
        if (d == QLatin1String("Full"))
            m_productCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.type_full")));
        else if (d == QLatin1String("Lang"))
            m_productCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.type_lang")));
        else if (d == QLatin1String("Proof"))
            m_productCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.type_proof")));
    }
    m_productCombo->blockSignals(false);

    m_outputCombo->blockSignals(true);
    for (int i = 0; i < m_outputCombo->count(); ++i) {
        const QString d = m_outputCombo->itemData(i).toString();
        if (d == QLatin1String("aria2"))
            m_outputCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.format_aria2")));
        else if (d == QLatin1String("curl"))
            m_outputCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.format_curl")));
        else if (d == QLatin1String("wget"))
            m_outputCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.format_wget")));
        else if (d == QLatin1String("text"))
            m_outputCombo->setItemText(i, Locale::tr(QStringLiteral("office.yaoctru.format_text")));
    }
    m_outputCombo->blockSignals(false);

    m_previewEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px; }")
        .arg(Theme::Color::Surface().name(QColor::HexRgb),
             Theme::Color::TextPrimary().name(QColor::HexRgb),
             Theme::Color::BorderControl().name(QColor::HexRgb)));

    updateFreeSpace();
}

void YaoctruDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal rad = Theme::Metric::ControlRadius + 4.0;

    p.setPen(Theme::Color::BorderControl());
    p.setBrush(Theme::Color::Window());
    p.drawRoundedRect(r, rad, rad);
}

void YaoctruDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (parentWidget()) {
        const QRect parentRect = parentWidget()->geometry();
        const int x = parentRect.x() + (parentRect.width() - width()) / 2;
        int y = parentRect.y() + qMax(16, (parentRect.height() - height()) / 2);
        if (y + height() > parentRect.bottom() - 12) {
            y = qMax(parentRect.top() + 16, parentRect.bottom() - height() - 12);
        }
        move(x, y);
    }
}
