// debloatrow.h — one installed app in the Debloat page.
//
// Taller than a SettingRow (56px vs ~40) because the whole point of this list is a real
// logo pulled from the package itself, and a 30px mark needs the room. A checkbox on the
// left feeds the page's bulk-remove bar; the row's own Kaldır button removes just this
// one — both end up calling the same backend script with a one-entry list.

#pragma once

#include <QPixmap>
#include <QWidget>

class PillButton;

class DebloatRow : public QWidget
{
    Q_OBJECT

public:
    DebloatRow(const QString &id, const QPixmap &logo, const QString &name,
               const QString &desc, QWidget *parent = nullptr);

    QString id() const { return m_id; }
    bool checked() const { return m_checked && !m_locked; }
    void setChecked(bool on);

    /// A package Windows itself marks as non-removable: shown for completeness, but with
    /// no checkbox and no button, because there is nothing here the user may do to it.
    void setLocked(bool locked);
    bool locked() const { return m_locked; }

    /// Disables the row's own button while a removal touching it is in flight.
    void setBusy(bool busy);

    /// Replaces the description with a result line (e.g. after a removal). The row is
    /// rebuilt on the next rescan, which is what puts the original text back.
    void setStatus(const QString &text);

    PillButton *removeButton() const { return m_removeButton; }

    QSize sizeHint() const override;

Q_SIGNALS:
    void toggled(const QString &id, bool checked);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QRectF checkboxRect() const;
    /// Sizes the button from its hint and sits it at the right edge. From resizeEvent, and
    /// again when the typeface or text size changes: that changes the button's width
    /// without changing the row's, so no resize arrives to move it.
    void placeButton();

    QString m_id;
    QPixmap m_logo;
    QString m_name;
    QString m_desc;
    QString m_status;
    bool m_checked = false;
    bool m_hovered = false;
    bool m_locked = false;
    PillButton *m_removeButton = nullptr;
};
