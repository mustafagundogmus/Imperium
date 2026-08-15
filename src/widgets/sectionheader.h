// sectionheader.h — the rule-and-label header shared by tweak sections and the
// Genel Bakış info blocks.
//
//   padding 0 6px 6px · gap 10px
//   label   10px uppercase, letter-spacing .09em, #6E6E78, weight 500
//   rule    flex:1, height 1px, #1D1D22
//   count   IBM Plex Mono 9.5px #45454E   (tweak sections only)

#pragma once

#include <QWidget>

class SectionHeader : public QWidget
{
    Q_OBJECT

public:
    explicit SectionHeader(const QString &title, QWidget *parent = nullptr);

    void setCount(const QString &text);   ///< e.g. "4 öğe"; empty hides it

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_title;   ///< already upper-cased for Turkish
    QString m_count;
};
