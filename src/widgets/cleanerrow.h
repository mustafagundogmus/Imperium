// cleanerrow.h — one target in the Disk cleaner page.
//
// A checkbox, a name, a description that becomes the result line once a clean has run,
// and the size on the right in the mono figure face — the one number the page is about.
// The shape is DebloatRow's without the logo; it is its own class rather than a flag on
// that one because the two lists say different things on the right-hand side and neither
// should have to know about the other's.

#pragma once

#include <QWidget>

class CleanerRow : public QWidget
{
    Q_OBJECT

public:
    CleanerRow(const QString &id, const QString &name, const QString &desc,
               QWidget *parent = nullptr);

    QString id() const { return m_id; }
    bool checked() const { return m_checked; }
    void setChecked(bool on);

    /// The figure on the right: "1,2 GB", "—" while unmeasured.
    void setSize(const QString &text);

    /// Replaces the description with a result or progress line. Empty puts the
    /// description back.
    void setStatus(const QString &text);
    void setDescription(const QString &text);

    /// Dims the row and ignores clicks while a clean touching it is in flight.
    void setBusy(bool busy);

    QSize sizeHint() const override;

Q_SIGNALS:
    void toggled(const QString &id, bool checked);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QRectF checkboxRect() const;

    QString m_id;
    QString m_name;
    QString m_desc;
    QString m_status;
    QString m_size;
    bool m_checked = false;
    bool m_hovered = false;
    bool m_busy = false;
};
