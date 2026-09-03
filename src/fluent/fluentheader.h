// fluentheader.h — §4's top block: the content column's header.
//
//   padding 28 36 0 · gap 16
//   breadcrumb 12px textMuted "Tweakler › Gizlilik", the last crumb textSec
//   H1 28px/600, letter-spacing -.01em, line-height 1.15
//   subtitle 13px textSec
//   at the right, bottom-aligned with the text: the profile button
//   below, the segmented filter: controlBg, 1px controlBorder, radius 5, padding 3,
//   26px segments (padding 0 12, radius 3, 12px; selected card + text + a soft shadow,
//   idle textSec), each with its 11px mono count at 70%
//
// The pages that are not a tweak list hide the profile button and the filter, as the
// classic header does.

#pragma once

#include <QStringList>
#include <QWidget>

class FluentButton;

class FluentSegmented : public QWidget
{
    Q_OBJECT

public:
    explicit FluentSegmented(const QStringList &labels, QWidget *parent = nullptr);

    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    void setLabels(const QStringList &labels);
    void setCounts(const QVector<int> &counts);

    QSize sizeHint() const override;

Q_SIGNALS:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    qreal segmentWidth(int index) const;
    int segmentAt(const QPointF &pos) const;
    void refreshGeometry();

    QStringList m_labels;
    QVector<int> m_counts;
    int m_current = 0;
    int m_hovered = -1;
    int m_pressed = -1;
};

class FluentHeader : public QWidget
{
    Q_OBJECT

public:
    explicit FluentHeader(QWidget *parent = nullptr);

    void setBreadcrumb(const QString &parent, const QString &page);
    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);
    void setControlsVisible(bool visible);
    void setFilterCounts(int all, int changed, int enabled);
    void setFilterIndex(int index);

    QSize sizeHint() const override;

Q_SIGNALS:
    /// 0 Tümü, 1 Değiştirilen, 2 Etkin — the handoff's order, not AppState's.
    void filterChanged(int index);
    void profileRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void relayout();
    qreal textBlockHeight() const;

    QString m_parent;
    QString m_page;
    QString m_title;
    QString m_subtitle;
    bool m_controlsVisible = true;
    FluentButton *m_profile = nullptr;
    FluentSegmented *m_filter = nullptr;
};
