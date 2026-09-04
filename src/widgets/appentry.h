// appentry.h — one program tile on the Uygulama kur page.
//
// WinUtil's Initialize-InstallAppEntry, in this app's vocabulary: a fixed-width tile
// that flows in a wrap panel, a checkbox, an icon box with the first letter of the name
// (WinUtil fetches the site's favicon from Google and falls back to this letter; a
// privacy tool has no business fetching two hundred favicons, so the fallback is the
// design), the name, and — for free software — the green corner badge with the open
// source keyhole. The whole tile is the click target, the tooltip carries the
// description and the preset key, and a right click asks the page for its Install /
// Uninstall / Info popup.
//
// A tile has no state of its own beyond checked: the page owns the selection and the
// tile draws it, the same contract every row in this app keeps.

#pragma once

#include <QWidget>

class AppEntry : public QWidget
{
    Q_OBJECT

public:
    AppEntry(const QString &key, const QString &name, bool foss, QWidget *parent = nullptr);

    QString key() const { return m_key; }
    QString name() const { return m_name; }

    bool checked() const { return m_checked; }
    void setChecked(bool on);

    /// Dims the tile while a run that covers it is in flight.
    void setBusy(bool busy);
    bool busy() const { return m_busy; }

    /// A short result word drawn at the right edge — "kuruldu", "çıkış 1" — in the
    /// colour the outcome deserves. Empty clears it.
    void setStatus(const QString &text, bool good);

    /// The tile's width at the current interface scale; every tile shares it so the
    /// flow reads as a grid.
    static int tileWidth();
    static int tileHeight();

    QSize sizeHint() const override;

Q_SIGNALS:
    void toggled(const QString &key, bool checked);
    void contextRequested(const QString &key, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    QString m_key;
    QString m_name;
    QString m_status;
    bool m_statusGood = true;
    bool m_foss = false;
    bool m_checked = false;
    bool m_hovered = false;
    bool m_busy = false;
};
