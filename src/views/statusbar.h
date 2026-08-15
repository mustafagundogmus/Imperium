// statusbar.h — §5 of the handoff.
//
//   36px tall, top border 1px #1D1D22, padding 0 12px 0 18px, gap 10px
//   left   : "194/194 tweak yüklendi · profil: varsayılan"  mono 10px #55555E, flexes
//   then   : "N değişiklik bekliyor"                        11px #9A9AA3
//   then   : "Geri al"    ghost pill
//   then   : "Uygula (N)" accent pill

#pragma once

#include <QWidget>

class PillButton;

class StatusBar : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setSummary(const QString &summary);
    void setPending(int count);

    /// Replaces the left-hand summary with \a text for a few seconds. Used to report
    /// what an "Uygula" actually did without opening a dialog for the ordinary case.
    void setNotice(const QString &text);

    QSize sizeHint() const override;

Q_SIGNALS:
    void revertRequested();
    void applyRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    QString m_summary;
    QString m_notice;
    QString m_pendingText;
    PillButton *m_revert = nullptr;
    PillButton *m_apply = nullptr;
    class QTimer *m_noticeTimer = nullptr;
};
