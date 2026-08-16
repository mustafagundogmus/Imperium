// settings.h — the preferences that are not part of the design tokens.
//
// Theme, accent and density live in Theme because widgets read them while painting;
// everything else the settings page offers lives here. All of it is stored through
// QSettings, i.e. under HKCU\Software\Arbitrium\Arbitrium — the app's own key, never
// anywhere a tweak could reach.

#pragma once

#include <QObject>

class Settings : public QObject
{
    Q_OBJECT

public:
    static Settings &instance();

    bool smoothScroll() const { return m_smoothScroll; }
    void setSmoothScroll(bool on);

    bool borderGlow() const { return m_borderGlow; }
    void setBorderGlow(bool on);

    bool checkUpdatesOnLaunch() const { return m_checkUpdatesOnLaunch; }
    void setCheckUpdatesOnLaunch(bool on);

    bool confirmBeforeApply() const { return m_confirmBeforeApply; }
    void setConfirmBeforeApply(bool on);

Q_SIGNALS:
    void smoothScrollChanged(bool on);
    void borderGlowChanged(bool on);
    void changed();

private:
    Settings();

    bool m_smoothScroll = true;
    bool m_borderGlow = true;
    bool m_checkUpdatesOnLaunch = false;
    bool m_confirmBeforeApply = true;
};
