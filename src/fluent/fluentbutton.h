// fluentbutton.h — the two push affordances of the Fluent shell, from the handoff's
// "Uygula çubuğu" and "Kontroller" sections.
//
//   Secondary  32px tall · padding 0 14 · controlBg · 1px controlBorder · radius 4 · 13px
//              hover controlHover
//   Primary    32px tall · padding 0 18 · accent · onAccent 13px/500 · radius 4
//              optional 14px check glyph before the text · hover brightness 1.08
//
// Both dim to 50% and stop answering the mouse when setEnabledLook(false) — "Bekleyen
// yoksa Vazgeç/Uygula opacity .5 ve pasif".

#pragma once

#include <QString>
#include <QWidget>

class FluentButton : public QWidget
{
    Q_OBJECT

public:
    enum Variant { Secondary, Primary };

    FluentButton(Variant variant, const QString &text, QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const { return m_text; }

    /// A 24-viewBox stroke path drawn before the text at \a size px — the apply bar's check
    /// mark, the profile button's sliders. Empty removes it.
    void setLeadingIcon(const QString &pathData, int size, qreal stroke);

    /// A 12px chevron after the text, for the profile button.
    void setTrailingChevron(bool on);

    /// The handoff's row action button is 30px where the bar buttons are 32.
    void setButtonHeight(int height);

    void setEnabledLook(bool enabled);

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
    qreal padX() const;
    QFont font() const;

    Variant m_variant;
    QString m_text;
    QString m_icon;
    int m_iconSize = 0;
    qreal m_iconStroke = 2.0;
    bool m_chevron = false;
    int m_height = 32;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_live = true;
};
