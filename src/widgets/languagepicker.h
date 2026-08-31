// languagepicker.h — the interface language, as a grid of chips.
//
// Ten native names of ten different lengths, so every chip is cut to the width of the
// widest of them and the strip becomes a grid whose columns line up. It wraps to as many
// columns as the width it is handed can hold, and then evens the cells out over the rows
// that needs — see Css::flexColumns — so the settings page's full content column shows
// all ten on one line while the setup wizard's narrower one shows a tidy 5×2, and neither
// of them ever elides a language away.
//
// Same chip as TypefacePicker, and exactly as tall as a segment of SegmentedControl,
// which is this app's one pill height: it is derived from the type, so the chips grow
// with the interface scale instead of staying at the size they were built at.

#pragma once

#include <QVector>
#include <QWidget>

class LanguagePicker : public QWidget
{
    Q_OBJECT

public:
    explicit LanguagePicker(QWidget *parent = nullptr);

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
        QRectF box;
    };

    /// Reads the language table and measures the one cell size every chip is cut to.
    /// Only the font can change this, so it runs at build and on a typeface change.
    void measure();
    /// Places the chips for the width the widget currently has.
    void relayout();
    int columnsFor(int width) const;
    int indexAt(const QPointF &pos) const;

    QVector<Chip> m_chips;
    qreal m_cell = 0.0;    ///< uniform chip width at the current font
    qreal m_chipH = 0.0;   ///< the app's pill height at the current font
    int m_hovered = -1;
    int m_pressed = -1;
};
