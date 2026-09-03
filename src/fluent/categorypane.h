// categorypane.h — §3 of the Fluent handoff: the 232px pane between the rail and the
// content.
//
//   padding 4 8 12 4 · gap 8
//   the search box (32px)
//   a heading ("KATEGORİLER") 11px/600 uppercase, letter-spacing .04em, textMuted,
//   padding 6 12 4, then the rows — 36px, radius 4, padding 0 12, 2px apart, label 13px
//   text with an 11px mono count at the right, selected `selected` + a 3×16 accent bar
//   10px down, hover subtleHover — in a scroll area
//   the system status card, pinned at the bottom: card, 1px cardBorder, radius 6,
//   padding 12, gap 10 — three live meters, then the restore point and its "Oluştur"
//
// Which rows it shows is the chrome's decision; the pane is a list with a search box
// above it and a status card below.

#pragma once

#include <QVector>
#include <QWidget>

class FluentSearchBox;
class SmoothScrollArea;
class StatusCard;
struct Sample;
namespace Icons { struct Glyph; }

class PaneRow : public QWidget
{
    Q_OBJECT

public:
    /// \a glyph is drawn at 16px before the label — the classic sidebar's rows carry one
    /// and the pane's read better with it than the handoff's bare labels did.
    PaneRow(const QString &id, const QString &label, const QString &count,
            const Icons::Glyph *glyph, QWidget *parent = nullptr);

    QString id() const { return m_id; }
    void setSelected(bool on);
    void setLabel(const QString &label);
    void setCount(const QString &count);

    static constexpr int Height = 36;

Q_SIGNALS:
    void activated(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QString m_id;
    QString m_label;
    QString m_count;
    const Icons::Glyph *m_glyph = nullptr;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

class CategoryPane : public QWidget
{
    Q_OBJECT

public:
    struct Item
    {
        QString id;
        QString label;
        QString count;
        const Icons::Glyph *glyph = nullptr;
    };

    explicit CategoryPane(QWidget *parent = nullptr);

    FluentSearchBox *search() const { return m_search; }

    /// Replaces the list under \a heading (already in the interface language; it is
    /// upper-cased here).
    void setItems(const QString &heading, const QVector<Item> &items);
    void setSelected(const QString &id);
    void setCount(const QString &id, const QString &count);

    void setSample(const Sample &sample);
    void setRestorePoint(const QString &text);

Q_SIGNALS:
    void activated(const QString &id);
    void restorePointRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void relayout();

    FluentSearchBox *m_search = nullptr;
    SmoothScrollArea *m_scroll = nullptr;
    QWidget *m_list = nullptr;
    StatusCard *m_status = nullptr;
    QString m_heading;
    QVector<PaneRow *> m_rows;
    QString m_selected;
};

/// The "SİSTEM DURUMU" card at the bottom of the pane.
class StatusCard : public QWidget
{
    Q_OBJECT

public:
    explicit StatusCard(QWidget *parent = nullptr);

    void setSample(const Sample &sample);
    void setRestorePoint(const QString &text);

    QSize sizeHint() const override;

Q_SIGNALS:
    void createRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF linkRect() const;

    qreal m_cpu = 0.0;
    qreal m_ram = 0.0;
    qreal m_disk = 0.0;
    QString m_cpuText;
    QString m_ramText;
    QString m_diskText;
    QString m_restore;
    bool m_linkHovered = false;
    bool m_linkPressed = false;
    bool m_live = false;
};
