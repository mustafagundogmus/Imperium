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
//
// It is a flyout as well. While the pointer rests on it, it widens to 200px over the pane
// beside it — 180ms, eased — and each button becomes a row with its name beside the icon,
// the names sliding in as it opens; it narrows again the moment the pointer leaves. For
// that it lives outside the chrome's layout, as an overlay the chrome positions (see
// FluentChrome::placeRail): the chrome owns where it is and how tall, the rail alone owns
// its width. A pointer merely crossing the rail on its way to the pane does not open it —
// the opening waits 140ms.

#pragma once

#include <QVector>
#include <QWidget>

class QTimer;
class QVariantAnimation;
namespace Icons { struct Glyph; }

class IconRail : public QWidget
{
    Q_OBJECT

public:
    struct Entry
    {
        QString label;                          ///< the row's name when open; the tooltip when not
        const Icons::Glyph *glyph = nullptr;    ///< lucide, drawn at 18px
    };

    explicit IconRail(QWidget *parent = nullptr);

    void setEntries(const QVector<Entry> &entries, const Entry &settings);
    void setLabels(const QStringList &labels, const QString &settingsLabel);

    /// -1 selects the settings cog.
    void setSelected(int index);
    int selected() const { return m_selected; }

    static constexpr int ExpandedWidth = 200;

Q_SIGNALS:
    void activated(int index);   ///< -1 for the settings cog

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    bool event(QEvent *) override;

private:
    /// The row of a button: 40×40 when closed, the rail's width less the margins when
    /// open. -1 for the settings cog.
    QRectF rowRect(int index) const;
    int indexAt(const QPointF &pos) const;   ///< -2 for none
    void slideTo(qreal progress);
    void setProgress(qreal progress);

    QVector<Entry> m_entries;
    Entry m_settings;
    int m_selected = 0;
    int m_hovered = -2;
    int m_pressed = -2;
    qreal m_progress = 0.0;                  ///< 0 closed … 1 open
    QVariantAnimation *m_slide = nullptr;
    QTimer *m_openDelay = nullptr;
};
