#include "settings.h"

#include <QSettings>

namespace {
const QString KeySmoothScroll = QStringLiteral("ui/smoothScroll");
const QString KeyBorderGlow = QStringLiteral("ui/borderGlow");
const QString KeyCheckUpdates = QStringLiteral("app/checkUpdatesOnLaunch");
const QString KeyConfirmApply = QStringLiteral("app/confirmBeforeApply");
} // namespace

Settings &Settings::instance()
{
    static Settings s;
    return s;
}

Settings::Settings()
{
    QSettings store;
    m_smoothScroll = store.value(KeySmoothScroll, true).toBool();
    m_borderGlow = store.value(KeyBorderGlow, true).toBool();
    m_checkUpdatesOnLaunch = store.value(KeyCheckUpdates, false).toBool();
    // Applying writes to the registry, so the confirmation is on unless the user says no.
    m_confirmBeforeApply = store.value(KeyConfirmApply, true).toBool();
}

void Settings::setSmoothScroll(bool on)
{
    if (m_smoothScroll == on)
        return;
    m_smoothScroll = on;
    QSettings().setValue(KeySmoothScroll, on);
    Q_EMIT smoothScrollChanged(on);
    Q_EMIT changed();
}

void Settings::setBorderGlow(bool on)
{
    if (m_borderGlow == on)
        return;
    m_borderGlow = on;
    QSettings().setValue(KeyBorderGlow, on);
    Q_EMIT borderGlowChanged(on);
    Q_EMIT changed();
}

void Settings::setCheckUpdatesOnLaunch(bool on)
{
    if (m_checkUpdatesOnLaunch == on)
        return;
    m_checkUpdatesOnLaunch = on;
    QSettings().setValue(KeyCheckUpdates, on);
    Q_EMIT changed();
}

void Settings::setConfirmBeforeApply(bool on)
{
    if (m_confirmBeforeApply == on)
        return;
    m_confirmBeforeApply = on;
    QSettings().setValue(KeyConfirmApply, on);
    Q_EMIT changed();
}
