// tweakpage.h — the scrollable tweak list (§4 of the handoff).
//
// The scroll body carries the design's asymmetric padding (2px 12px 16px 18px) and a
// 16px gap between sections; rows inside a section are 1px apart.

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
    AppState *m_state = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QWidget *m_body = nullptr;
};
