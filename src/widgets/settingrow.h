// settingrow.h — one line in the settings page.
//
// Deliberately the same geometry as TweakRow (padding 7px 6px, 12px column gap, name at
// 12.5px over a 10.5px description) so the settings page reads as part of the same list
// system rather than as a separate dialog. Two arrangements:
//
//   Leading   [ 26px control ] gap 12 [ name / desc ]        — switches
//   Trailing  [ name / desc ................. ] gap 16 [ control ]  — choices, buttons

#pragma once

#include <QWidget>

class SettingRow : public QWidget
{
    Q_OBJECT

public:
    enum Placement { Leading, Trailing };

    SettingRow(const QString &name, const QString &desc, QWidget *control,
               Placement placement, QWidget *parent = nullptr);

    /// Rows with a taller control (a swatch strip, a two-button pair) grow to fit it.
    static int rowHeight();

    void setDesc(const QString &desc);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void positionControl();

    QString m_name;
    QString m_desc;
    QWidget *m_control = nullptr;
    Placement m_placement;
};
