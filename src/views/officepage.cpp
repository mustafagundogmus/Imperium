#include "officepage.h"

#include "../i18n.h"
#include "../officedownloader.h"
#include "../theme.h"
#include "../widgets/actioncard.h"
#include "../widgets/dialog.h"
#include "../widgets/downloadprogresswidget.h"
#include "../widgets/officeimagedialog.h"
#include "../widgets/officepackagedialog.h"
#include "../widgets/yaoctrudialog.h"
#include "../widgets/sectionheader.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const QString SvgDownloadPkg = QStringLiteral("M5 20H19V18H5V20M19 9H15V3H9V9H5L12 16L19 9Z");
const QString SvgDownloadImg = QStringLiteral(
    "M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2"
    "M12 20C7.59 20 4 16.41 4 12C4 7.59 7.59 4 12 4C16.41 4 20 7.59 20 12C20 16.41 16.41 20 12 20"
    "M12 10.5C11.17 10.5 10.5 11.17 10.5 12C10.5 12.83 11.17 13.5 12 13.5C12.83 13.5 13.5 12.83 13.5 12C13.5 11.17 12.83 10.5 12 10.5Z");
const QString SvgYaoctru = QStringLiteral(
    "M3.9 12C3.9 10.29 5.29 8.9 7 8.9H11V7H7C4.24 7 2 9.24 2 12C2 14.76 4.24 17 7 17H11V15.1H7C5.29 15.1 3.9 13.71 3.9 12"
    "M8 13H16V11H8V13"
    "M17 7H13V8.9H17C18.71 8.9 20.1 10.29 20.1 12C20.1 13.71 18.71 15.1 17 15.1H13V17H17C19.76 17 22 14.76 22 12C22 9.24 19.76 7 17 7Z");
const QString SvgInstallPkg = QStringLiteral(
    "M5 3C3.89 3 3 3.9 3 5V19C3 20.11 3.9 21 5 21H19C20.11 21 21 20.11 21 19V5C21 3.9 20.11 3 19 3H5"
    "M11.5 14.5L7.5 10.5L8.91 9.09L11.5 11.67L15.59 7.58L17 9L11.5 14.5Z");
const QString SvgInstallImg = QStringLiteral(
    "M11.99 2L2 7L12 12L22 7L11.99 2M2 17L12 22L22 17V12L12 17L2 12V17Z");

} // namespace

OfficePage::OfficePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Theme::Metric::PagePadLeft, Theme::Metric::PagePadTop,
                               Theme::Metric::PagePadRight, Theme::Metric::PagePadBottom);
    layout->setSpacing(8);

    // ─── 0. BÖLÜM: AKTİF İNDİRME İLERLEME PANELİ (Baştan gizli) ───
    m_downloader = new OfficeDownloader(this);
    m_progressWidget = new DownloadProgressWidget(this);
    m_progressWidget->setDownloader(m_downloader);
    m_progressWidget->hide();
    layout->addWidget(m_progressWidget);

    connect(m_progressWidget, &DownloadProgressWidget::openFolderRequested, this, [](const QString &filePath) {
        if (!filePath.isEmpty()) {
            const QString dir = QFileInfo(filePath).isDir() ? filePath : QFileInfo(filePath).absolutePath();
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        }
    });

    connect(m_progressWidget, &DownloadProgressWidget::mountRequested, this, [this](const QString &filePath) {
        if (!filePath.isEmpty()) {
            const QString cmdPath = QDir(filePath).filePath(QStringLiteral("start_setup.cmd"));
            if (QFile::exists(cmdPath)) {
                QProcess::startDetached(QStringLiteral("cmd.exe"), {
                    QStringLiteral("/c"),
                    QStringLiteral("start"),
                    QStringLiteral("cmd.exe"),
                    QStringLiteral("/c"),
                    cmdPath
                });
                Q_EMIT notice(Locale::tr(QStringLiteral("office.progress.start_setup")));
            } else {
                QProcess::startDetached(QStringLiteral("powershell.exe"), {
                    QStringLiteral("-NoProfile"),
                    QStringLiteral("-Command"),
                    QStringLiteral("Mount-DiskImage -ImagePath \"%1\"").arg(filePath)
                });
                Q_EMIT notice(QStringLiteral("İmaj Windows'a bağlanıyor..."));
            }
        }
    });

    connect(m_downloader, &OfficeDownloader::finished, this, [this](bool ok, const QString &, const QString &err) {
        if (ok) {
            Q_EMIT notice(Locale::tr(QStringLiteral("office.progress.completed")));
        } else if (!err.isEmpty()) {
            Q_EMIT notice(QStringLiteral("Hata: %1").arg(err));
        }
    });

    // ─── 1. BÖLÜM: DOWNLOAD ───
    m_downloadHeader = new SectionHeader(Locale::tr(QStringLiteral("office.section.download")), this);
    layout->addWidget(m_downloadHeader);

    m_cardDownloadPkg = new ActionCard(Locale::tr(QStringLiteral("office.card.download_pkg.title")),
                                       Locale::tr(QStringLiteral("office.card.download_pkg.desc")),
                                       SvgDownloadPkg, this);
    connect(m_cardDownloadPkg, &ActionCard::clicked, this, [this] { onCardClicked(0); });
    layout->addWidget(m_cardDownloadPkg);

    m_cardDownloadImg = new ActionCard(Locale::tr(QStringLiteral("office.card.download_img.title")),
                                       Locale::tr(QStringLiteral("office.card.download_img.desc")),
                                       SvgDownloadImg, this);
    connect(m_cardDownloadImg, &ActionCard::clicked, this, [this] { onCardClicked(1); });
    layout->addWidget(m_cardDownloadImg);

    m_cardYaoctru = new ActionCard(Locale::tr(QStringLiteral("office.card.yaoctru.title")),
                                   Locale::tr(QStringLiteral("office.card.yaoctru.desc")),
                                   SvgYaoctru, this);
    connect(m_cardYaoctru, &ActionCard::clicked, this, [this] { onCardClicked(2); });
    layout->addWidget(m_cardYaoctru);

    layout->addSpacing(14);

    // ─── 2. BÖLÜM: INSTALL ───
    m_installHeader = new SectionHeader(Locale::tr(QStringLiteral("office.section.install")), this);
    layout->addWidget(m_installHeader);

    m_cardInstallPkg = new ActionCard(Locale::tr(QStringLiteral("office.card.install_pkg.title")),
                                      Locale::tr(QStringLiteral("office.card.install_pkg.desc")),
                                      SvgInstallPkg, this);
    connect(m_cardInstallPkg, &ActionCard::clicked, this, [this] { onCardClicked(3); });
    layout->addWidget(m_cardInstallPkg);

    m_cardInstallImg = new ActionCard(Locale::tr(QStringLiteral("office.card.install_img.title")),
                                      Locale::tr(QStringLiteral("office.card.install_img.desc")),
                                      SvgInstallImg, this);
    connect(m_cardInstallImg, &ActionCard::clicked, this, [this] { onCardClicked(4); });
    layout->addWidget(m_cardInstallImg);

    layout->addStretch(1);

    connect(Locale::notifier(), &Locale::Notifier::languageChanged, this, &OfficePage::retranslate);
}

void OfficePage::retranslate()
{
    m_downloadHeader->setTitle(Locale::tr(QStringLiteral("office.section.download")));
    m_cardDownloadPkg->setTitle(Locale::tr(QStringLiteral("office.card.download_pkg.title")));
    m_cardDownloadPkg->setDesc(Locale::tr(QStringLiteral("office.card.download_pkg.desc")));
    m_cardDownloadImg->setTitle(Locale::tr(QStringLiteral("office.card.download_img.title")));
    m_cardDownloadImg->setDesc(Locale::tr(QStringLiteral("office.card.download_img.desc")));
    m_cardYaoctru->setTitle(Locale::tr(QStringLiteral("office.card.yaoctru.title")));
    m_cardYaoctru->setDesc(Locale::tr(QStringLiteral("office.card.yaoctru.desc")));

    m_installHeader->setTitle(Locale::tr(QStringLiteral("office.section.install")));
    m_cardInstallPkg->setTitle(Locale::tr(QStringLiteral("office.card.install_pkg.title")));
    m_cardInstallPkg->setDesc(Locale::tr(QStringLiteral("office.card.install_pkg.desc")));
    m_cardInstallImg->setTitle(Locale::tr(QStringLiteral("office.card.install_img.title")));
    m_cardInstallImg->setDesc(Locale::tr(QStringLiteral("office.card.install_img.desc")));
}

void OfficePage::onCardClicked(int cardIndex)
{
    if (cardIndex == 0) {
        // Çevrimdışı Kurulum Paketi İndir (C2R klasör yapısı)
        OfficePackageDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString channelGuid = dlg.selectedChannelGuid();
            const QString channelName = dlg.selectedChannelName();
            const QString arch = dlg.selectedArch();
            const QString lang = dlg.selectedLangTag();
            const QString dir = dlg.selectedOutputDir();
            const QString tool = dlg.selectedTool();

            m_progressWidget->reset();
            m_progressWidget->show();
            m_downloader->startPackageDownload(channelGuid, channelName, arch, lang, dir, tool);

            Q_EMIT notice(Locale::tr(QStringLiteral("office.progress.downloading")) + QStringLiteral(": ") + channelName);
        }
        return;
    }

    if (cardIndex == 1) {
        // Çevrimdışı Kurulum İmajını İndir (.ISO / .IMG)
        OfficeImageDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString url = dlg.cdnUrl();
            const QString dir = dlg.selectedOutputDir();
            const QString fileName = dlg.generatedFileName();
            const QString tool = dlg.selectedTool();

            m_progressWidget->reset();
            m_progressWidget->show();
            m_downloader->startDownload(url, dir, fileName, tool);

            Q_EMIT notice(Locale::tr(QStringLiteral("office.progress.downloading")) + QStringLiteral(": ") + fileName);
        }
        return;
    }

    if (cardIndex == 2) {
        // YAOCTRU Bağlantı & Betik Üretici
        YaoctruDialog dlg(this);
        dlg.exec();
        return;
    }

    // Henüz implemente edilmemiş kartlar (install_pkg, install_img)
    QString cardName;
    switch (cardIndex) {
    case 3: cardName = m_cardInstallPkg->title(); break;
    case 4: cardName = m_cardInstallImg->title(); break;
    default: return;
    }

    Q_EMIT notice(Locale::tr(QStringLiteral("office.notice.selected")).arg(cardName));
}
