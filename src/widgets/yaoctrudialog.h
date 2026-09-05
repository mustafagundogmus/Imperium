#ifndef YAOCTRUDIALOG_H
#define YAOCTRUDIALOG_H

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QLabel;
class QPlainTextEdit;
class QNetworkAccessManager;
class PillButton;

class YaoctruDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YaoctruDialog(QWidget *parent = nullptr);
    ~YaoctruDialog() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
    void onOsLevelChanged(int index);
    void onChannelChanged(int index);
    void onGenerateClicked();
    void onCopyClicked();
    void onSaveClicked();
    void onRunClicked();

private:
    void setupUi();
    void retranslate();
    void updateFreeSpace();
    void updateOsCombo();
    void updateChannelCombo();
    void updateArchCombo();
    void generateScript(const QString &build);

    // Helpers
    static QString ensureCurlExtracted();
    static QStringList generateUrls(const QString &channelGuid, const QString &version,
                                    const QString &lang, int lcid, const QString &bitness,
                                    const QString &productType, QString &outArch, QString &outBit);

    static QString generateAria2Script(const QStringList &urls, const QString &channelName,
                                       const QString &version, const QString &arch, const QString &lang,
                                       const QString &prodType, const QString &osLevel);
    static QString generateCurlScript(const QStringList &urls, const QString &channelName,
                                      const QString &version, const QString &arch, const QString &lang,
                                      const QString &prodType, const QString &osLevel);
    static QString generateWgetScript(const QStringList &urls, const QString &channelName,
                                      const QString &version, const QString &arch, const QString &lang,
                                      const QString &prodType, const QString &osLevel);

    QNetworkAccessManager *m_nam = nullptr;

    // Form controls
    QComboBox *m_osCombo = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QComboBox *m_archCombo = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_productCombo = nullptr;
    QComboBox *m_outputCombo = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    PillButton *m_browseBtn = nullptr;
    QLabel *m_freeSpaceLabel = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_descLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    PillButton *m_cancelBtn = nullptr;
    PillButton *m_generateBtn = nullptr;

    // Result / Preview section
    QWidget *m_resultWidget = nullptr;
    QLabel *m_resultFileLabel = nullptr;
    QPlainTextEdit *m_previewEdit = nullptr;
    PillButton *m_copyBtn = nullptr;
    PillButton *m_saveBtn = nullptr;
    PillButton *m_runBtn = nullptr;

    QString m_generatedFileName;
    QString m_generatedContent;
    QString m_savedFilePath;
};

#endif // YAOCTRUDIALOG_H
