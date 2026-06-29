// SPDX-License-Identifier: GPL-2.0-or-later

#include "ui/SettingsController.h"

#include "core/SafetyDefaults.h"
#include "core/ScheduleTime.h"

namespace lumacore {

SettingsController::SettingsController(QObject* parent)
    : QObject(parent)
{
    load();
}

bool SettingsController::animationsEnabled() const
{
    return m_animationsEnabled;
}

void SettingsController::setAnimationsEnabled(bool enabled)
{
    if (m_animationsEnabled == enabled) {
        return;
    }

    m_animationsEnabled = enabled;
    m_settings.setValue(QStringLiteral("ui/animationsEnabled"), enabled);
    emit animationsEnabledChanged();
}

bool SettingsController::reduceVrrFlicker() const
{
    return m_reduceVrrFlicker;
}

void SettingsController::setReduceVrrFlicker(bool enabled)
{
    if (m_reduceVrrFlicker == enabled) {
        return;
    }

    m_reduceVrrFlicker = enabled;
    m_settings.setValue(QStringLiteral("ui/reduceVrrFlicker"), enabled);
    emit reduceVrrFlickerChanged();
}

bool SettingsController::startMinimized() const
{
    return m_startMinimized;
}

void SettingsController::setStartMinimized(bool enabled)
{
    if (m_startMinimized == enabled) {
        return;
    }

    m_startMinimized = enabled;
    m_settings.setValue(QStringLiteral("startup/startMinimized"), enabled);
    emit startMinimizedChanged();
}

bool SettingsController::closeToTray() const
{
    return m_closeToTray;
}

void SettingsController::setCloseToTray(bool enabled)
{
    if (m_closeToTray == enabled) {
        return;
    }

    m_closeToTray = enabled;
    m_settings.setValue(QStringLiteral("startup/closeToTray"), enabled);
    emit closeToTrayChanged();
}

bool SettingsController::trayAvailable() const
{
    return m_trayAvailable;
}

void SettingsController::setTrayAvailable(bool available)
{
    if (m_trayAvailable == available) {
        return;
    }

    m_trayAvailable = available;
    emit trayAvailableChanged();
}

bool SettingsController::applyOnLaunch() const
{
    return m_applyOnLaunch;
}

void SettingsController::setApplyOnLaunch(bool enabled)
{
    const bool sanitizedEnabled = enabled && !m_activeProfile.isEmpty();
    if (m_applyOnLaunch == sanitizedEnabled) {
        return;
    }

    m_applyOnLaunch = sanitizedEnabled;
    m_settings.setValue(QStringLiteral("startup/applyOnLaunch"), sanitizedEnabled);
    emit applyOnLaunchChanged();
}

bool SettingsController::scheduledProfileEnabled() const
{
    return m_scheduledProfileEnabled;
}

void SettingsController::setScheduledProfileEnabled(bool enabled)
{
    const bool sanitizedEnabled = enabled && !m_scheduledProfile.isEmpty();
    if (m_scheduledProfileEnabled == sanitizedEnabled) {
        return;
    }

    m_scheduledProfileEnabled = sanitizedEnabled;
    m_settings.setValue(QStringLiteral("schedule/enabled"), sanitizedEnabled);
    emit scheduledProfileEnabledChanged();
}

QString SettingsController::scheduledProfile() const
{
    return m_scheduledProfile;
}

void SettingsController::setScheduledProfile(const QString& profileName)
{
    const QString sanitizedProfileName = profileName.trimmed();
    if (m_scheduledProfile == sanitizedProfileName) {
        return;
    }

    m_scheduledProfile = sanitizedProfileName;
    if (m_scheduledProfile.isEmpty()) {
        m_settings.remove(QStringLiteral("schedule/profile"));
        setScheduledProfileEnabled(false);
    } else {
        m_settings.setValue(QStringLiteral("schedule/profile"), m_scheduledProfile);
    }
    emit scheduledProfileChanged();
}

QString SettingsController::scheduledProfileTime() const
{
    return m_scheduledProfileTime;
}

void SettingsController::setScheduledProfileTime(const QString& time)
{
    const QString sanitizedTime = normalizeScheduleTime(time);
    if (m_scheduledProfileTime == sanitizedTime) {
        return;
    }

    m_scheduledProfileTime = sanitizedTime;
    m_settings.setValue(QStringLiteral("schedule/time"), sanitizedTime);
    emit scheduledProfileTimeChanged();
}

bool SettingsController::dryRunEnabled() const
{
    return m_dryRunEnabled;
}

void SettingsController::setDryRunEnabled(bool enabled)
{
    if (m_dryRunEnabled == enabled) {
        return;
    }

    m_dryRunEnabled = enabled;
    m_settings.setValue(QStringLiteral("safety/dryRunEnabled"), enabled);
    emit dryRunEnabledChanged();
}

QString SettingsController::theme() const
{
    return m_theme;
}

namespace {

QString normalizeTheme(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Dark");
    }
    // Legacy value migration.
    if (trimmed.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Auto");
    }
    if (trimmed.compare(QStringLiteral("Auto"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Auto");
    }
    if (trimmed.compare(QStringLiteral("Light"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Light");
    }
    return QStringLiteral("Dark");
}

} // namespace

void SettingsController::setTheme(const QString& theme)
{
    const QString sanitizedTheme = normalizeTheme(theme);
    if (m_theme == sanitizedTheme) {
        return;
    }

    m_theme = sanitizedTheme;
    m_settings.setValue(QStringLiteral("ui/theme"), sanitizedTheme);
    emit themeChanged();
}

QString SettingsController::activeProfile() const
{
    return m_activeProfile;
}

void SettingsController::setActiveProfile(const QString& profileName)
{
    const QString sanitizedProfileName = profileName.trimmed();
    if (m_activeProfile == sanitizedProfileName) {
        return;
    }

    m_activeProfile = sanitizedProfileName;
    if (m_activeProfile.isEmpty()) {
        m_settings.remove(QStringLiteral("startup/activeProfile"));
        setApplyOnLaunch(false);
    } else {
        m_settings.setValue(QStringLiteral("startup/activeProfile"), m_activeProfile);
    }
    emit activeProfileChanged();
}

void SettingsController::load()
{
    m_animationsEnabled = m_settings.value(QStringLiteral("ui/animationsEnabled"), true).toBool();
    m_reduceVrrFlicker = m_settings.value(QStringLiteral("ui/reduceVrrFlicker"), true).toBool();
    m_startMinimized = m_settings.value(QStringLiteral("startup/startMinimized"), false).toBool();
    m_closeToTray = m_settings.value(QStringLiteral("startup/closeToTray"), false).toBool();
    m_dryRunEnabled = m_settings.value(QStringLiteral("safety/dryRunEnabled"), platformDefaultDryRunEnabled()).toBool();
    m_theme = normalizeTheme(m_settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString());
    m_activeProfile = m_settings.value(QStringLiteral("startup/activeProfile")).toString().trimmed();
    m_applyOnLaunch = !m_activeProfile.isEmpty()
        && m_settings.value(QStringLiteral("startup/applyOnLaunch"), false).toBool();
    m_scheduledProfile = m_settings.value(QStringLiteral("schedule/profile")).toString().trimmed();
    m_scheduledProfileTime = normalizeScheduleTime(
        m_settings.value(QStringLiteral("schedule/time"), QStringLiteral("18:00")).toString()
    );
    m_scheduledProfileEnabled = !m_scheduledProfile.isEmpty()
        && m_settings.value(QStringLiteral("schedule/enabled"), false).toBool();
}

} // namespace lumacore
