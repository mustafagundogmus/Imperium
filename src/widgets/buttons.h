// buttons.h — the three push affordances in the design.
//
//   PillButton::Ghost   "Geri al"    11px #7A7A84, border 1px #26262C, radius 5,
//                                    padding 3px 10px, hover text #C6C6CE
//   PillButton::Accent  "Uygula (3)" 11px/500 #141414 on the accent, radius 5,
//                                    padding 4px 12px, hover filter brightness(1.08)
//   WindowButton        40×36 hit area, 10px glyph, hover #1C1C21 (close: #3A1D1F)

#pragma once

#include <QPixmap>
#include <QWidget>

class PillButton : public QWidget
{
    Q_OBJECT

public:
    enum Variant { Ghost, Accent };

    PillButton(Variant variant, const QString &text, QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const { return m_text; }

    void setEnabledLook(bool enabled);   ///< dims the button without disabling hover math

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
    void refreshGeometry();

    Variant m_variant;
    QString m_text;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_live = true;
};

class WindowButton : public QWidget
{
    Q_OBJECT

public:
    enum Kind { Minimize, Maximize, Restore, Close };

    explicit WindowButton(Kind kind, QWidget *parent = nullptr);

    void setKind(Kind kind);

    QSize sizeHint() const override;

Q_SIGNALS:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QPixmap glyph() const;

    Kind m_kind;
    bool m_hovered = false;
    bool m_pressed = false;
};
