#include "ui/SettingsController.h"

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

bool SettingsController::applyOnLaunch() const
{
    return m_applyOnLaunch;
}

void SettingsController::setApplyOnLaunch(bool enabled)
{
    if (m_applyOnLaunch == enabled) {
        return;
    }

    m_applyOnLaunch = enabled;
    m_settings.setValue(QStringLiteral("startup/applyOnLaunch"), enabled);
    emit applyOnLaunchChanged();
}

QString SettingsController::theme() const
{
    return m_theme;
}

void SettingsController::setTheme(const QString& theme)
{
    const QString sanitizedTheme = theme.trimmed().isEmpty() ? QStringLiteral("Dark") : theme.trimmed();
    if (m_theme == sanitizedTheme) {
        return;
    }

    m_theme = sanitizedTheme;
    m_settings.setValue(QStringLiteral("ui/theme"), sanitizedTheme);
    emit themeChanged();
}

void SettingsController::load()
{
    m_animationsEnabled = m_settings.value(QStringLiteral("ui/animationsEnabled"), true).toBool();
    m_startMinimized = m_settings.value(QStringLiteral("startup/startMinimized"), false).toBool();
    m_applyOnLaunch = m_settings.value(QStringLiteral("startup/applyOnLaunch"), false).toBool();
    m_theme = m_settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString();
}

} // namespace lumacore
