// contentheader.h — §3 of the handoff.
//
//   padding 14px 18px 10px · gap 12px
//   left  : title 15px/600/-.01em · sub 11px #6E6E78 · pending 11px accent, all sharing
//           one baseline, 10px apart
//   right : the segmented filter, then a 13px sort glyph
//
// On Genel Bakış the filter, the sort glyph and the pending label are hidden, exactly as
// the mockup's `notOv` guard does — which also makes the header 2px shorter there.

#pragma once

#include <QWidget>

class SegmentedControl;

class ContentHeader : public QWidget
{
    Q_OBJECT

public:
    explicit ContentHeader(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);
    void setPendingLabel(const QString &label);   ///< "3 bekliyor"; empty hides it
    void setControlsVisible(bool visible);
    void setFilterIndex(int index);

    QSize sizeHint() const override;

Q_SIGNALS:
    void filterChanged(int index);
    void sortToggled(bool alphabetical);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF sortRect() const;
    qreal contentHeight() const;

    QString m_title;
    QString m_subtitle;
    QString m_pending;
    bool m_controlsVisible = true;
    bool m_sortActive = false;
    bool m_sortHovered = false;
    SegmentedControl *m_filter = nullptr;
};
