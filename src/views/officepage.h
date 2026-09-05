// officepage.h - Microsoft Office İndirme ve Kurulum yönetim ekranı.

#pragma once

#include <QWidget>

class ActionCard;
class SectionHeader;
class QVBoxLayout;
class OfficeDownloader;
class DownloadProgressWidget;

class OfficePage : public QWidget
{
    Q_OBJECT

public:
    explicit OfficePage(QWidget *parent = nullptr);

Q_SIGNALS:
    void notice(const QString &text);

private:
    void retranslate();
    void onCardClicked(int cardIndex);

    OfficeDownloader *m_downloader = nullptr;
    DownloadProgressWidget *m_progressWidget = nullptr;

    SectionHeader *m_downloadHeader = nullptr;
    ActionCard *m_cardDownloadPkg = nullptr;
    ActionCard *m_cardDownloadImg = nullptr;
    ActionCard *m_cardYaoctru = nullptr;

    SectionHeader *m_installHeader = nullptr;
    ActionCard *m_cardInstallPkg = nullptr;
    ActionCard *m_cardInstallImg = nullptr;
};
