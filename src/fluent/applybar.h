// applybar.h — §5 of the Fluent handoff: the 56px bar under the content column.
//
//   mica · top 1px cardBorder · padding 0 36 · gap 12
//   left  : an 8px dot — accent while something is pending, textMuted otherwise — and
//           "N değişiklik uygulanmayı bekliyor" / "Tüm değişiklikler uygulandı" 13px textSec
//   right : Günlük (secondary) · Vazgeç (secondary) · Uygula (primary, with the check)
//           Vazgeç and Uygula dim to 50% and go quiet while nothing is pending
//
// A notice takes the sentence's place for a few seconds, the way StatusBar does it.

#pragma once

#include <QWidget>

class FluentButton;
class QTimer;

class ApplyBar : public QWidget
{
    Q_OBJECT

public:
    explicit ApplyBar(QWidget *parent = nullptr);

    void setPending(int count);
    void setNotice(const QString &text);

    /// Whether the bottom-right corner is the window's — it is clipped to the window
    /// radius when it is, and square when the window is maximised.
    void setCornerRadius(qreal radius);

    QSize sizeHint() const override;

Q_SIGNALS:
    void journalRequested();
    void revertRequested();
    void applyRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    int m_pending = 0;
    QString m_notice;
    qreal m_corner = 0.0;
    FluentButton *m_journal = nullptr;
    FluentButton *m_revert = nullptr;
    FluentButton *m_apply = nullptr;
    QTimer *m_noticeTimer = nullptr;
};
