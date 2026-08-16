// overviewblocks.h — the two building blocks of the Genel Bakış screen.
//
// StatTile   a card: border 1px, radius 5, tile background, padding 10px 12px
//            label 10.5px uppercase .08em · value IBM Plex Mono 15px · meter · sub 10px
//            The meter is the part the eye reads first, so it is only drawn for the
//            tiles that are a proportion of something (cpu, memory, disk).
//
// InfoSection  a card of the same family: a 12.5px semibold title over a rule, then
//              label/value rows on 1px separators. A row can carry a meter of its own —
//              a volume is a proportion too, and a bar says it faster than "378 GB boş".
//
// Both were flat text on the window background before; the design reads as a dashboard
// now, which is what the page had become.

#pragma once

#include <QVector>
#include <QWidget>

class StatTile : public QWidget
{
    Q_OBJECT

public:
    StatTile(const QString &label, QWidget *parent = nullptr);

    void setLabel(const QString &label);
    void setValue(const QString &value);
    void setSub(const QString &sub);

    /// 0…1 draws the bar, a negative value hides it. Tiles are laid out to the same
    /// height either way, so a page can mix them without the row going ragged.
    void setMeter(qreal fraction);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_label;   ///< already upper-cased
    QString m_value;
    QString m_sub;
    qreal m_meter = -1.0;
};

struct InfoRow
{
    QString label;
    QString value;
    bool mono = false;    ///< technical values (version, date, time, BIOS) use the mono face
    qreal meter = -1.0;   ///< 0…1 draws a usage bar under the row
};

class InfoSection : public QWidget
{
    Q_OBJECT

public:
    InfoSection(const QString &title, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setRows(const QVector<InfoRow> &rows);

    /// Updates one value in place — used by the rows that tick (uptime).
    void setRowValue(int index, const QString &value);

    /// A short note on the right of the title, e.g. "4 birim".
    void setNote(const QString &note);

    QSize sizeHint() const override;

    static qreal rowHeight(bool withMeter = false);
    static qreal headerHeight();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    qreal contentHeight() const;

    QString m_title;
    QString m_note;
    QVector<InfoRow> m_rows;
};
