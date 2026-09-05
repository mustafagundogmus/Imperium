// actioncard.h - OfficeRTool_GUI tarzında tıklanabilir geniş eylem kartı.

#pragma once

#include <QWidget>

class ActionCard : public QWidget
{
    Q_OBJECT

public:
    explicit ActionCard(const QString &title, const QString &desc, const QString &svgPath,
                        QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setDesc(const QString &desc);
    void setIconPath(const QString &svgPath);

    QString title() const { return m_title; }
    QString desc() const { return m_desc; }

    QSize sizeHint() const override;

Q_SIGNALS:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QString m_title;
    QString m_desc;
    QString m_svgPath;
    bool m_hovered = false;
    bool m_pressed = false;
};
