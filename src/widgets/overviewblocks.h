// overviewblocks.h — the two building blocks of the Genel Bakış screen.
//
// StatTile   border 1px #1F1F24 · radius 5 · background #141417 · padding 10px 12px
//            column gap 3px: label 10px uppercase .08em #6E6E78
//                            value IBM Plex Mono 15px #E8E8EA
//                            sub   10px #55555E
//
// InfoSection  section header (§4 style, no count) followed by label/value rows:
//              padding 5px 6px · border-bottom 1px #17171B · align-items:baseline
//              label 11px #77777F · value right aligned, either mono 10.5px or 11px, #C6C6CE

#pragma once

#include <QVector>
#include <QWidget>

class SectionHeader;

class StatTile : public QWidget
{
    Q_OBJECT

public:
    StatTile(const QString &label, QWidget *parent = nullptr);

    void setValue(const QString &value);
    void setSub(const QString &sub);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_label;   ///< already upper-cased
    QString m_value;
    QString m_sub;
};

struct InfoRow
{
    QString label;
    QString value;
    bool mono = false;   ///< technical values (version, date, time, BIOS) use the mono face
};

class InfoSection : public QWidget
{
    Q_OBJECT

public:
    InfoSection(const QString &title, QWidget *parent = nullptr);

    void setRows(const QVector<InfoRow> &rows);

    /// Updates one value in place — used by the rows that tick (uptime).
    void setRowValue(int index, const QString &value);

    QSize sizeHint() const override;

    static qreal rowHeight();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    SectionHeader *m_header = nullptr;
    QVector<InfoRow> m_rows;
};
