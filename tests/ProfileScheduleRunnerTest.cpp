// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/ProfileScheduleRunner.h"
#include "core/DeviceManager.h"
#include "ui/AppController.h"
#include "ui/SettingsController.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTime>
#include <QTimeZone>

#include <memory>
#include <vector>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

void processEventsFor(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

class ScheduleDevice final : public lumacore::RgbDevice
{
public:
    ScheduleDevice()
        : RgbDevice(
              QStringLiteral("schedule-device"),
              QStringLiteral("Schedule Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Header"),
            lumacore::RgbZoneType::AddressableHeader,
            1
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        if (zoneIndex < 0 || zoneIndex >= mutableZones().size()) {
            return false;
        }
        mutableZones()[zoneIndex].setColor(color);
        return true;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneColorWrite
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported schedule test capability.")};
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    using lumacore::ProfileScheduleRunner;

    if (!require(
            ProfileScheduleRunner::parseScheduledTime(QStringLiteral("07:30")) == QTime(7, 30),
            "valid scheduled times should parse"
        )
        || !require(
            ProfileScheduleRunner::parseScheduledTime(QStringLiteral("invalid")) == QTime(18, 0),
            "invalid scheduled times should fall back to 18:00"
        )) {
        return 1;
    }

    const QDate date(2026, 6, 21);
    const QTimeZone utc = QTimeZone::utc();
    const QDateTime beforeRun(date, QTime(7, 0), utc);
    const QDateTime afterRun(date, QTime(8, 0), utc);

    if (!require(
            ProfileScheduleRunner::millisecondsUntilNextRun(beforeRun, QTime(7, 30))
                == 30 * 60 * 1000,
            "next run should be later today when the scheduled time is ahead"
        )
        || !require(
            ProfileScheduleRunner::millisecondsUntilNextRun(afterRun, QTime(7, 30))
                == 23 * 60 * 60 * 1000 + 30 * 60 * 1000,
            "next run should roll to tomorrow after today's scheduled time has passed"
        )
        || !require(
            ProfileScheduleRunner::millisecondsUntilNextRun(QDateTime(), QTime(7, 30)) == 0,
            "invalid current times should not schedule a delayed run"
        )
        || !require(
            ProfileScheduleRunner::millisecondsUntilNextRun(beforeRun, QTime()) == 0,
            "invalid scheduled times should not schedule a delayed run"
        )) {
        return 1;
    }

    QTemporaryDir settingsDirectory;
    QTemporaryDir profilesDirectory;
    if (!require(settingsDirectory.isValid(), "temporary settings directory should be available")
        || !require(profilesDirectory.isValid(), "temporary profiles directory should be available")) {
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("LumaCoreTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ProfileScheduleRunnerTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    lumacore::DeviceManager manager(nullptr, profilesDirectory.filePath(QStringLiteral("profiles")));
    std::vector<std::unique_ptr<lumacore::RgbDevice>> devices;
    devices.push_back(std::make_unique<ScheduleDevice>());
    manager.replaceDevices(std::move(devices));
    lumacore::AppController controller(&manager);

    const QColor scheduledColor(QStringLiteral("#112233"));
    const QColor currentColor(QStringLiteral("#445566"));
    if (!require(
            controller.applyEffect(0, 0, static_cast<int>(lumacore::RgbEffectType::Static), scheduledColor, 1.0, 100),
            "scheduled profile fixture color should apply"
        )
        || !require(controller.saveProfile(QStringLiteral("Scheduled")), "scheduled profile fixture should save")
        || !require(
            controller.applyEffect(0, 0, static_cast<int>(lumacore::RgbEffectType::Static), currentColor, 1.0, 100),
            "current fixture color should apply"
        )) {
        return 1;
    }

    lumacore::SettingsController settings;
    settings.setScheduledProfile(QStringLiteral("Scheduled"));
    settings.setScheduledProfileTime(QStringLiteral("00:00"));
    settings.setScheduledProfileEnabled(true);

    lumacore::ProfileScheduleRunner runner(&settings, &controller);
    processEventsFor(100);
    if (!require(
            controller.zoneEffectColor(0, 0) == currentColor,
            "missed scheduled profiles should not apply immediately on runner startup"
        )) {
        return 1;
    }

    return 0;
}
