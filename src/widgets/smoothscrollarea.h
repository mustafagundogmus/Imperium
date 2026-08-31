// smoothscrollarea.h — scroll container for the main content column.
//
// Carries the mockup's scrollbar skin — a transparent track the width of
// Theme::Metric::ScrollBarWidth, holding a thumb in the palette's scroll colour inset by a
// 2px window-coloured border and capped at half the track — and eases wheel scrolling
// instead of jumping in three-line steps.
//
// The two numbers are named rather than written down here because they moved once already:
// the mockup's 8px track left a 4px thumb after the border, and a comment that repeats a
// constant is a comment that will disagree with it.

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
