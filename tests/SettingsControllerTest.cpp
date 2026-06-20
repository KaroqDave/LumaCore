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
        if (!require(!settings.reduceVrrFlicker(), "VRR flicker reduction should be opt-in by default")) {
            return 1;
        }

        settings.setReduceVrrFlicker(true);
        if (!require(settings.reduceVrrFlicker(), "VRR flicker setting should update immediately")) {
            return 1;
        }
    }

    {
        lumacore::SettingsController settings;
        if (!require(settings.reduceVrrFlicker(), "VRR flicker setting should persist when enabled")) {
            return 1;
        }

        settings.setReduceVrrFlicker(false);
    }

    lumacore::SettingsController settings;
    if (!require(!settings.reduceVrrFlicker(), "VRR flicker setting should persist when disabled")) {
        return 1;
    }

    return 0;
}
