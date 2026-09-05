// officeimagedialog.h - Çevrimdışı Office İmajı indirme parametre seçim penceresi.

#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QLabel;
class PillButton;

class OfficeImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OfficeImageDialog(QWidget *parent = nullptr);

    QString selectedProductFileName() const;
    QString selectedProductName() const;
    QString selectedLangTag() const;
    QString selectedTool() const;
    QString selectedOutputDir() const;
    QString cdnUrl() const;
    QString generatedFileName() const;

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void updateFreeSpace();
    void retranslate();

    QComboBox *m_productCombo = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_toolCombo = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLabel *m_freeSpaceLabel = nullptr;

    QLabel *m_productLabel = nullptr;
    QLabel *m_langLabel = nullptr;
    QLabel *m_toolLabel = nullptr;
    QLabel *m_pathLabel = nullptr;

    PillButton *m_browseBtn = nullptr;
    PillButton *m_startBtn = nullptr;
    PillButton *m_cancelBtn = nullptr;
};
