// categoryrow.h — one sidebar entry from §2 of the handoff.
//
//   height 28px · radius 5px · padding 0 10px · gap 9px
//   icon  12×12, 1px stroke in currentColor
//   name  12.5px, weight 450 (#A3A3AC) → weight 500 in the accent colour when selected
//   count IBM Plex Mono 10px #55555E, right aligned
//   hover background #18181D · selected background = accent at 13%

#pragma once

#include <QWidget>

class CategoryRow : public QWidget
{
    Q_OBJECT

public:
    CategoryRow(const QString &id, const QString &name, const QString &iconPath,
                const QString &count, QWidget *parent = nullptr);

    QString categoryId() const { return m_id; }

    bool isSelected() const { return m_selected; }
    void setSelected(bool on);

    void setCount(const QString &count);

    QSize sizeHint() const override;

Q_SIGNALS:
    void activated(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QString m_id;
    QString m_name;
    QString m_iconPath;
    QString m_count;
    bool m_selected = false;
    bool m_hovered = false;
};
