// fluentcontent.h — the content column of the Fluent shell.
//
//   `surface` fill · top-left corner radius 8 · 1px cardBorder along the top and the
//   left — the Windows 11 Settings look, the pane and rail on mica beside and above it
//
// Holds the header, the page stack and the apply bar in a column; the stack is inset
// 18px each side so the pages' own 18px gutters come to the handoff's 36.

#pragma once

#include <QWidget>

class ApplyBar;
class FluentHeader;

class FluentContent : public QWidget
{
    Q_OBJECT

public:
    FluentContent(FluentHeader *header, QWidget *stack, ApplyBar *bar, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};
