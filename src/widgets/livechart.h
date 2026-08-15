// livechart.h — the rolling CPU / memory chart on Genel Bakış.
//
// Same chrome as a stat tile (border 1px #1F1F24, radius 5, background #141417) so the
// panel reads as part of the same family: a legend row on top, then a 0–100% plot of the
// last sixty samples with a 1px stroke and a soft fade underneath.

#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class LiveChart : public QWidget
{
    Q_OBJECT

public:
    explicit LiveChart(QWidget *parent = nullptr);

    /// \a capacity is the full window width in samples; shorter series are drawn
    /// right-aligned so the newest value always sits at the right edge.
    void setSeries(const QVector<qreal> &cpu, const QVector<qreal> &ram, int capacity);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void paintSeries(QPainter &p, const QRectF &plot, const QVector<qreal> &values,
                     const QColor &colour, int capacity) const;

    QVector<qreal> m_cpu;
    QVector<qreal> m_ram;
    int m_capacity = 60;
};
