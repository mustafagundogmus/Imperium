// typefacepicker.h — the interface face, as a grid of chips.
//
// Each chip is its own name set in its own face, so the strip is the specimen sheet: you
// pick by reading, not by imagining. The selected chip carries the accent wash the
// sidebar uses for a selected category.
//
// Every chip is cut to the width of the widest specimen and wrapped through
// Css::flexColumns, the same rule the language grid and the theme cards follow — six
// names measured in six different faces are six different widths, and lining the columns
// up is what stops the strip reading as a pile.
//
// The specimens are all set at one size so the faces are compared against each other and
// not against six different sizes. That size follows the interface scale: fixing it
// against the scale as well left this the one control on the page that ignored
// "Çok büyük" entirely.
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
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

Q_SIGNALS:
    void picked(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
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

    /// Loads the faces and measures the one cell every chip is cut to. Only the interface
    /// scale can change this, so it runs at build and on a typeface change.
    void measure();
    /// Places the chips for the width the widget currently has.
    void relayout();
    int columnsFor(int width) const;
    int indexAt(const QPointF &pos) const;

    QVector<Chip> m_chips;
    qreal m_cell = 0.0;    ///< uniform chip width, from the widest specimen
    qreal m_chipH = 0.0;   ///< the app's pill height, or the tallest specimen if that is taller
    int m_hovered = -1;
    int m_pressed = -1;
};
