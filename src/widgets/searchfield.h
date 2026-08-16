// searchfield.h — the sidebar search box from §2 of the handoff.
//
//   height 27px · border 1px #26262C · radius 5px · background #16161A · padding 0 8px
//   gap 7px · magnifier 11px · placeholder "Tweak ara…" 11.5px #5A5A64
//   trailing "⌃K" badge: mono 9px, border 1px #26262C, radius 3px, padding 1px 4px
//
// The mockup is static and therefore has no focus state; a 1px border lift to #33333A
// is added here because a real text field needs one. Nothing else is invented.

#pragma once

#include <QWidget>

class QLineEdit;

class SearchField : public QWidget
{
    Q_OBJECT

public:
    explicit SearchField(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);
    void clearText();
    void focusField();

private Q_SLOTS:
    void applyStyle();

public:

    QSize sizeHint() const override;

Q_SIGNALS:
    void textChanged(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    qreal badgeWidth() const;
    void layoutEditor();
    int preferredHeight() const;

    QLineEdit *m_edit = nullptr;
    bool m_focused = false;
};
