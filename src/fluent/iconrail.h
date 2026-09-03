// iconrail.h — §2 of the Fluent handoff: the 56px rail of 40×40 buttons.
//
//   padding-top 4 · gap 2 · radius 5 · icon 18px, stroke 1.75
//   selected: `selected` fill, icon in text, a 3×16 accent bar at the left (radius 2,
//             12px down)
//   idle    : icon textSec · hover subtleHover
//   the settings cog sits apart at the bottom, 8px up
//
// The rail knows nothing about pages: it shows the entries it is given and says which
// was pressed. The chrome maps an entry to the pane it opens.

#pragma once

#include <QVector>
#include <QWidget>

namespace Icons { struct Glyph; }

class IconRail : public QWidget
{
    Q_OBJECT

public:
    struct Entry
    {
        QString label;                          ///< tooltip
        const Icons::Glyph *glyph = nullptr;    ///< lucide, drawn at 18px
    };

    explicit IconRail(QWidget *parent = nullptr);

    void setEntries(const QVector<Entry> &entries, const Entry &settings);
    void setLabels(const QStringList &labels, const QString &settingsLabel);

    /// -1 selects the settings cog.
    void setSelected(int index);
    int selected() const { return m_selected; }

Q_SIGNALS:
    void activated(int index);   ///< -1 for the settings cog

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    bool event(QEvent *) override;

private:
    QRectF buttonRect(int index) const;   ///< -1 for the settings cog
    int indexAt(const QPointF &pos) const;   ///< -2 for none

    QVector<Entry> m_entries;
    Entry m_settings;
    int m_selected = 0;
    int m_hovered = -2;
    int m_pressed = -2;
};
