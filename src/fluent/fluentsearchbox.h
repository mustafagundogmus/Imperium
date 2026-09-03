// fluentsearchbox.h — the category pane's search box (§3 of the Fluent handoff).
//
//   32px · controlBg · 1px controlBorder with the bottom edge in textMuted (the Fluent
//   TextBox underline) · radius 4 · padding 0 10 · gap 8
//   14px magnifier textSec · placeholder "Tweak ara" 13px textMuted
//   trailing "Ctrl+K" badge: 10px mono textMuted, 1px controlBorder, radius 3, padding 0 4
//
// Same shape as SearchField underneath: a QLineEdit drawn inside a painted frame, so the
// window's Ctrl+K and Escape reach the same kind of thing whichever shell is on.

#pragma once

#include <QWidget>

class QLineEdit;

class FluentSearchBox : public QWidget
{
    Q_OBJECT

public:
    explicit FluentSearchBox(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);
    void clearText();
    void focusField();

    QSize sizeHint() const override;

Q_SIGNALS:
    void textChanged(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyStyle();
    void layoutEditor();
    qreal badgeWidth() const;

    QLineEdit *m_edit = nullptr;
    bool m_focused = false;
};
