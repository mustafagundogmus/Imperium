// smoothscrollarea.h — scroll container for the main content column.
//
// Carries the mockup's scrollbar skin (8px wide, #232327 thumb inset by a 2px
// window-coloured border, radius 4, transparent track) and eases wheel scrolling
// instead of jumping in three-line steps.

#pragma once

#include <QScrollArea>

class QPropertyAnimation;

class SmoothScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit SmoothScrollArea(QWidget *parent = nullptr);

    void scrollToTop();

private Q_SLOTS:
    /// The scrollbar skin is a stylesheet, so it has to be rebuilt when the palette changes.
    void applyStyle();

protected:
    void wheelEvent(QWheelEvent *) override;

private:
    QPropertyAnimation *m_anim = nullptr;
    int m_target = 0;
};
