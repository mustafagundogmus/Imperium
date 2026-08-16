// languagepicker.h — the interface language, as two rows of chips.
//
// Same chip language as TypefacePicker, but wrapped: ten languages in one row would run
// past any reasonable settings-column width, especially with wide native names like
// "Português" or "العربية" sitting next to one-word ones like "Polski". Wrapping at a
// fixed column count keeps the strip a predictable two rows regardless of window width.

#pragma once

#include <QVector>
#include <QWidget>

class LanguagePicker : public QWidget
{
    Q_OBJECT

public:
    explicit LanguagePicker(QWidget *parent = nullptr);

    QSize sizeHint() const override;

Q_SIGNALS:
    void picked(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    struct Chip
    {
        QString id;
        QString name;
        QRectF box;
    };

    void rebuild();
    int indexAt(const QPointF &pos) const;

    QVector<Chip> m_chips;
    int m_hovered = -1;
    int m_pressed = -1;
};
