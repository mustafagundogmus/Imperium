// officepackagedialog.h - Çevrimdışı Office Kurulum Paketi indirme parametre seçim penceresi.

#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QLabel;
class PillButton;

class OfficePackageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OfficePackageDialog(QWidget *parent = nullptr);

    QString selectedChannelGuid() const;
    QString selectedChannelName() const;
    QString selectedArch() const;
    QString selectedLangTag() const;
    QString selectedTool() const;
    QString selectedOutputDir() const;

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void updateFreeSpace();
    void retranslate();

    QComboBox *m_channelCombo = nullptr;
    QComboBox *m_archCombo = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_toolCombo = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLabel *m_freeSpaceLabel = nullptr;

    QLabel *m_channelLabel = nullptr;
    QLabel *m_archLabel = nullptr;
    QLabel *m_langLabel = nullptr;
    QLabel *m_toolLabel = nullptr;
    QLabel *m_pathLabel = nullptr;

    PillButton *m_browseBtn = nullptr;
    PillButton *m_startBtn = nullptr;
    PillButton *m_cancelBtn = nullptr;
};
