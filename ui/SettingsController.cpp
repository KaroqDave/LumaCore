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

void SettingsController::load()
{
    m_animationsEnabled = m_settings.value(QStringLiteral("ui/animationsEnabled"), true).toBool();
    m_reduceVrrFlicker = m_settings.value(QStringLiteral("ui/reduceVrrFlicker"), false).toBool();
    m_startMinimized = m_settings.value(QStringLiteral("startup/startMinimized"), false).toBool();
    m_applyOnLaunch = m_settings.value(QStringLiteral("startup/applyOnLaunch"), false).toBool();
    m_dryRunEnabled = m_settings.value(QStringLiteral("safety/dryRunEnabled"), false).toBool();
    m_theme = normalizeTheme(m_settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString());
}

} // namespace lumacore
