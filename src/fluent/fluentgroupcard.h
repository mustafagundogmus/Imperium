// fluentgroupcard.h — a section of the Fluent tweak list: its heading and its card.
//
//   heading  13px/600 title · 11px textMuted count ("4 tweak") · padding 0 4 4 · gap 8
//   card     card fill · 1px cardBorder · radius 6 · rows separated by 1px divider
//
// The card owns the divider lines and the corner clipping; the rows own their hover.

#pragma once

#include <QWidget>

class FluentGroupHeader : public QWidget
{
    Q_OBJECT

public:
    FluentGroupHeader(const QString &title, const QString &sub, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_title;
    QString m_sub;
};

class FluentGroupCard : public QWidget
{
    Q_OBJECT

public:
    explicit FluentGroupCard(QWidget *parent = nullptr);

    /// Appends a row. Rows are stacked with no gap; the divider is painted, not laid out.
    void addRow(QWidget *row);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    class QVBoxLayout *m_layout = nullptr;
    QVector<QWidget *> m_rows;
};
