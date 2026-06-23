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
        if (!require(settings.reduceVrrFlicker(), "VRR flicker reduction should be enabled by default")
            || !require(!settings.closeToTray(), "close-to-tray should be opt-in by default")
            || !require(!settings.trayAvailable(), "tray availability should default to false until detected")
            || !require(settings.activeProfile().isEmpty(), "active profile should be empty by default")
            || !require(!settings.scheduledProfileEnabled(), "profile scheduling should be disabled by default")
            || !require(settings.scheduledProfile().isEmpty(), "scheduled profile should be empty by default")
            || !require(
                settings.scheduledProfileTime() == QStringLiteral("18:00"),
                "scheduled profile time should default to 18:00"
            )) {
            return 1;
        }

        settings.setReduceVrrFlicker(true);
        settings.setCloseToTray(true);
        settings.setTrayAvailable(true);
        settings.setApplyOnLaunch(true);
        if (!require(!settings.applyOnLaunch(), "launch application should require an active profile")) {
            return 1;
        }

        settings.setActiveProfile(QStringLiteral("  Evening  "));
        settings.setApplyOnLaunch(true);
        settings.setScheduledProfile(QStringLiteral("  Evening  "));
        settings.setScheduledProfileTime(QStringLiteral("07:30"));
        settings.setScheduledProfileEnabled(true);
        if (!require(settings.reduceVrrFlicker(), "VRR flicker setting should update immediately")
            || !require(settings.closeToTray(), "close-to-tray should update immediately")
            || !require(settings.trayAvailable(), "tray availability should update at runtime")
            || !require(settings.activeProfile() == QStringLiteral("Evening"), "active profile should be trimmed")
            || !require(settings.applyOnLaunch(), "launch application should enable with an active profile")
            || !require(
                settings.scheduledProfile() == QStringLiteral("Evening"),
                "scheduled profile should be trimmed"
            )
            || !require(
                settings.scheduledProfileTime() == QStringLiteral("07:30"),
                "scheduled profile time should update"
            )
            || !require(
                settings.scheduledProfileEnabled(),
                "profile scheduling should enable with a selected profile"
            )) {
            return 1;
        }
    }

    {
        lumacore::SettingsController settings;
        if (!require(settings.reduceVrrFlicker(), "VRR flicker setting should persist when enabled")
            || !require(settings.closeToTray(), "close-to-tray should persist when enabled")
            || !require(
                settings.activeProfile() == QStringLiteral("Evening"),
                "active profile selection should persist"
            )
            || !require(settings.applyOnLaunch(), "launch application setting should persist")) {
            return 1;
        }
        if (!require(
                settings.scheduledProfile() == QStringLiteral("Evening"),
                "scheduled profile should persist"
            )
            || !require(
                settings.scheduledProfileTime() == QStringLiteral("07:30"),
                "scheduled profile time should persist"
            )
            || !require(settings.scheduledProfileEnabled(), "profile scheduling should persist")) {
            return 1;
        }

        settings.setReduceVrrFlicker(false);
        settings.setCloseToTray(false);
        settings.setActiveProfile(QString());
        settings.setScheduledProfileTime(QStringLiteral("25:99"));
        if (!require(
                settings.scheduledProfileTime() == QStringLiteral("18:00"),
                "invalid scheduled profile time should fall back to 18:00"
            )) {
            return 1;
        }
        settings.setScheduledProfile(QString());
        if (!require(!settings.applyOnLaunch(), "clearing the active profile should disable launch application")) {
            return 1;
        }
        if (!require(
                !settings.scheduledProfileEnabled(),
                "clearing the scheduled profile should disable profile scheduling"
            )) {
            return 1;
        }
    }

    lumacore::SettingsController settings;
    if (!require(!settings.reduceVrrFlicker(), "VRR flicker setting should persist when disabled")
        || !require(!settings.closeToTray(), "close-to-tray should persist when disabled")
        || !require(settings.activeProfile().isEmpty(), "cleared active profile should remain cleared")
        || !require(!settings.applyOnLaunch(), "disabled launch application should remain disabled")
        || !require(settings.scheduledProfile().isEmpty(), "cleared scheduled profile should remain cleared")
        || !require(!settings.scheduledProfileEnabled(), "disabled scheduling should remain disabled")
        || !require(
            settings.scheduledProfileTime() == QStringLiteral("18:00"),
            "normalized scheduled profile time should persist"
        )) {
        return 1;
    }

    return 0;
}
