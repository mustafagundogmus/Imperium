// featurerow.h — one row of the Özellikler page.
//
// The DebloatRow shape — checkbox, a 30px mark, name over description, a button at the
// right — with the mark a lucide glyph rather than a package logo, and a state word ahead
// of the description drawn the way TweakRow draws its risk badge: "etkin" in the accent
// ink, "kısmen" in amber, "devre dışı" and "bu sürümde yok" in the muted grey. A row whose
// feature this edition does not carry has no checkbox to tick.

#pragma once

#include <QWidget>

class PillButton;
namespace Icons { struct Glyph; }

class FeatureRow : public QWidget
{
    Q_OBJECT

public:
    enum class Tone { Muted, On, Warn, Danger };

    FeatureRow(const QString &key, const Icons::Glyph *glyph, QWidget *parent = nullptr);

    QString key() const { return m_key; }

    void setName(const QString &name);
    void setDesc(const QString &desc);

    /// The state word and its colour. Empty hides it.
    void setState(const QString &text, Tone tone);

    bool checked() const { return m_checked && m_selectable; }
    void setChecked(bool on);

    /// A row that cannot be ticked — the feature is not on this edition.
    void setSelectable(bool on);
    bool selectable() const { return m_selectable; }

    /// Dims the row's own button while a run touching it is in flight.
    void setBusy(bool busy);

    /// Replaces the description with a result line until the next scan.
    void setStatus(const QString &text);

    PillButton *actionButton() const { return m_button; }
    void setActionVisible(bool visible);

    QSize sizeHint() const override;

Q_SIGNALS:
    void toggled(const QString &key, bool checked);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QRectF checkboxRect() const;
    void placeButton();

    QString m_key;
    const Icons::Glyph *m_glyph = nullptr;
    QString m_name;
    QString m_desc;
    QString m_state;
    Tone m_tone = Tone::Muted;
    QString m_status;
    bool m_checked = false;
    bool m_hovered = false;
    bool m_selectable = true;
    bool m_actionVisible = false;
    PillButton *m_button = nullptr;
};
