// typefacepicker.h — the interface face, as a strip of chips.
//
// Each chip is its own name set in its own face, so the strip is the specimen sheet: you
// pick by reading, not by imagining. The selected chip carries the accent wash the
// sidebar uses for a selected category.
//
// Only the interface face changes. The mono face stays IBM Plex Mono throughout — the
// values it sets (versions, registry paths, byte counts) are column-aligned technical
// text and stop lining up the moment they are proportional.

#pragma once

#include <QVector>
#include <QWidget>

class TypefacePicker : public QWidget
{
    Q_OBJECT

public:
    explicit TypefacePicker(QWidget *parent = nullptr);

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
        QFont font;
        QRectF box;
    };

    void rebuild();
    int indexAt(const QPointF &pos) const;

    QVector<Chip> m_chips;
    int m_hovered = -1;
    int m_pressed = -1;
};
