// tweakpage.h — the scrollable tweak list (§4 of the handoff).
//
// The scroll body carries the design's asymmetric padding (2px 12px 16px 18px) and a
// 16px gap between sections; rows inside a section are 1px apart.
//
// Under the Fluent shell the same sections are drawn the Fluent handoff's way: a
// heading and a card per section, 20px apart, the rows inside the card separated by a
// hairline, in the 16 36 24 padding that shell gives its list. The page asks
// Theme::shell() each time it is rebuilt, which the window does on a shell switch.

#pragma once

#include <QVector>
#include <QWidget>

class AppState;
class QVBoxLayout;
struct Section;

class TweakPage : public QWidget
{
    Q_OBJECT

public:
    explicit TweakPage(AppState *state, QWidget *parent = nullptr);

    /// Replaces the whole list. Sections with no rows are skipped; an all-empty set
    /// shows \a emptyMessage.
    void setSections(const QVector<Section> &sections, const QString &emptyMessage = {});

private:
    void buildClassic(QVBoxLayout *body, const QVector<Section> &sections);
    void buildFluent(QVBoxLayout *body, const QVector<Section> &sections);

    AppState *m_state = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QWidget *m_body = nullptr;
};
