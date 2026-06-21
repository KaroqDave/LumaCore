#include "ui/SettingsController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir settingsDirectory;
    if (!require(settingsDirectory.isValid(), "temporary settings directory should be available")) {
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("LumaCoreTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsControllerTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    {
        lumacore::SettingsController settings;
        if (!require(!settings.reduceVrrFlicker(), "VRR flicker reduction should be opt-in by default")
            || !require(settings.activeProfile().isEmpty(), "active profile should be empty by default")) {
            return 1;
        }

        settings.setReduceVrrFlicker(true);
        settings.setApplyOnLaunch(true);
        if (!require(!settings.applyOnLaunch(), "launch application should require an active profile")) {
            return 1;
        }

        settings.setActiveProfile(QStringLiteral("  Evening  "));
        settings.setApplyOnLaunch(true);
        if (!require(settings.reduceVrrFlicker(), "VRR flicker setting should update immediately")
            || !require(settings.activeProfile() == QStringLiteral("Evening"), "active profile should be trimmed")
            || !require(settings.applyOnLaunch(), "launch application should enable with an active profile")) {
            return 1;
        }
    }

    {
        lumacore::SettingsController settings;
        if (!require(settings.reduceVrrFlicker(), "VRR flicker setting should persist when enabled")
            || !require(
                settings.activeProfile() == QStringLiteral("Evening"),
                "active profile selection should persist"
            )
            || !require(settings.applyOnLaunch(), "launch application setting should persist")) {
            return 1;
        }

        settings.setReduceVrrFlicker(false);
        settings.setActiveProfile(QString());
        if (!require(!settings.applyOnLaunch(), "clearing the active profile should disable launch application")) {
            return 1;
        }
    }

    lumacore::SettingsController settings;
    if (!require(!settings.reduceVrrFlicker(), "VRR flicker setting should persist when disabled")
        || !require(settings.activeProfile().isEmpty(), "cleared active profile should remain cleared")
        || !require(!settings.applyOnLaunch(), "disabled launch application should remain disabled")) {
        return 1;
    }

    return 0;
}
