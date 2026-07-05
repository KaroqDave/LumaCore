// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/DeviceManager.h"
#include "core/ScheduleService.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>
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

bool logContains(const lumacore::DeviceManager& manager, const QString& fragment)
{
    const QStringList lines = manager.activityLog().formattedLines();
    for (const QString& line : lines) {
        if (line.contains(fragment)) {
            return true;
        }
    }
    return false;
}

class ScheduleDevice final : public lumacore::RgbDevice
{
public:
    explicit ScheduleDevice(bool requiresConfirmation = false)
        : RgbDevice(
              QStringLiteral("schedule-service-device"),
              QStringLiteral("Schedule Service Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
        , m_requiresConfirmation(requiresConfirmation)
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
        if (!capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported schedule test capability.")};
        }
        if (m_requiresConfirmation && capability != lumacore::BackendCapability::DiscoveryRead) {
            return {
                lumacore::PermissionStatus::RequiresConfirmation,
                QStringLiteral("Schedule Service Device requires confirmation before hardware writes.")
            };
        }
        return {lumacore::PermissionStatus::Granted, {}};
    }

private:
    bool m_requiresConfirmation {false};
};

lumacore::RgbColor zoneColor(const lumacore::DeviceManager& manager)
{
    const lumacore::RgbDevice* device = manager.deviceAt(0);
    return device != nullptr ? device->zones().at(0).currentColor() : lumacore::RgbColor();
}

std::function<QDateTime()> clockAt(const QDate& date, const QTime& time)
{
    return [date, time]() { return QDateTime(date, time, QTimeZone::utc()); };
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    QTemporaryDir settingsDirectory;
    QTemporaryDir profilesDirectory;
    if (!require(settingsDirectory.isValid(), "temporary settings directory should be available")
        || !require(profilesDirectory.isValid(), "temporary profiles directory should be available")) {
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("LumaCoreTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ScheduleServiceTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    lumacore::DeviceManager manager(nullptr, profilesDirectory.filePath(QStringLiteral("profiles")));
    manager.setDryRunEnabled(false);

    // Config defaults, sanitization, and persistence round-trip.
    {
        lumacore::ScheduleService service(&manager);
        const lumacore::ScheduleService::Config defaults = service.config();
        if (!require(!defaults.enabled, "schedule should be disabled by default")
            || !require(defaults.profileName.isEmpty(), "scheduled profile should be empty by default")
            || !require(defaults.time == QStringLiteral("18:00"), "schedule time should default to 18:00")) {
            return 1;
        }

        lumacore::ScheduleService::Config invalid;
        invalid.enabled = true;
        invalid.profileName = QStringLiteral("   ");
        invalid.time = QStringLiteral("25:99");
        const lumacore::ScheduleService::Config sanitized = service.setConfig(invalid);
        if (!require(!sanitized.enabled, "enabling a schedule should require a profile name")
            || !require(sanitized.profileName.isEmpty(), "blank profile names should sanitize to empty")
            || !require(sanitized.time == QStringLiteral("18:00"), "invalid times should normalize to 18:00")) {
            return 1;
        }

        lumacore::ScheduleService::Config valid;
        valid.enabled = true;
        valid.profileName = QStringLiteral("  Evening  ");
        valid.time = QStringLiteral("07:30");
        const lumacore::ScheduleService::Config effective = service.setConfig(valid);
        if (!require(effective.enabled, "valid schedules should enable")
            || !require(effective.profileName == QStringLiteral("Evening"), "profile names should be trimmed")
            || !require(effective.time == QStringLiteral("07:30"), "valid times should be kept")) {
            return 1;
        }
    }
    {
        lumacore::ScheduleService reloaded(&manager);
        const lumacore::ScheduleService::Config persisted = reloaded.config();
        if (!require(persisted.enabled, "persisted schedules should stay enabled across restarts")
            || !require(
                persisted.profileName == QStringLiteral("Evening"),
                "persisted profile names should survive restarts"
            )
            || !require(persisted.time == QStringLiteral("07:30"), "persisted times should survive restarts")) {
            return 1;
        }
        reloaded.setConfig({});
    }

    // Fixture: a granted device and a saved profile holding the scheduled color.
    std::vector<std::unique_ptr<lumacore::RgbDevice>> devices;
    devices.push_back(std::make_unique<ScheduleDevice>());
    manager.replaceDevices(std::move(devices));

    const lumacore::RgbColor scheduledColor(0x11, 0x22, 0x33);
    const lumacore::RgbColor currentColor(0x44, 0x55, 0x66);
    if (!require(manager.setZoneStaticColor(0, 0, scheduledColor), "scheduled fixture color should apply")
        || !require(manager.saveProfile(QStringLiteral("Scheduled")), "scheduled fixture profile should save")
        || !require(manager.setZoneStaticColor(0, 0, currentColor), "current fixture color should apply")) {
        return 1;
    }

    const QDate dayOne(2026, 7, 1);
    const QDate dayTwo(2026, 7, 2);
    const QDate dayThree(2026, 7, 3);
    const QDate dayFour(2026, 7, 4);

    lumacore::ScheduleService service(&manager);
    lumacore::ScheduleService::Config config;
    config.enabled = true;
    config.profileName = QStringLiteral("Scheduled");
    config.time = QStringLiteral("00:30");
    service.setConfig(config);

    // Missed fires are skipped: the first evaluation past the boundary records
    // the attempt without applying.
    service.setClock(clockAt(dayOne, QTime(1, 0)));
    service.evaluateNow();
    if (!require(
            zoneColor(manager) == currentColor,
            "missed scheduled profiles should not apply on the first evaluation"
        )) {
        return 1;
    }

    // The next day's boundary crossing applies the profile.
    service.setClock(clockAt(dayTwo, QTime(0, 31)));
    service.evaluateNow();
    if (!require(zoneColor(manager) == scheduledColor, "scheduled profiles should apply at the next boundary")
        || !require(
            logContains(manager, QStringLiteral("Scheduled profile 'Scheduled' fired (daemon schedule).")),
            "daemon schedule fires should be logged"
        )) {
        return 1;
    }

    // Only one attempt per calendar day.
    if (!require(manager.setZoneStaticColor(0, 0, currentColor), "reset color should apply")) {
        return 1;
    }
    service.setClock(clockAt(dayTwo, QTime(12, 0)));
    service.evaluateNow();
    if (!require(zoneColor(manager) == currentColor, "scheduled profiles should fire once per day")) {
        return 1;
    }

    // Config changes reset the missed-run guard instead of retro-firing.
    service.setClock(clockAt(dayThree, QTime(9, 0)));
    service.setConfig(config);
    service.evaluateNow();
    if (!require(
            zoneColor(manager) == currentColor,
            "config changes should not retro-apply an already-passed boundary"
        )) {
        return 1;
    }
    service.setClock(clockAt(dayFour, QTime(0, 31)));
    service.evaluateNow();
    if (!require(zoneColor(manager) == scheduledColor, "the boundary after a config change should apply")) {
        return 1;
    }

    // Unconfirmed hardware is refused by the write gate and reported clearly.
    std::vector<std::unique_ptr<lumacore::RgbDevice>> gatedDevices;
    gatedDevices.push_back(std::make_unique<ScheduleDevice>(true));
    manager.replaceDevices(std::move(gatedDevices));
    if (!require(manager.confirmDeviceWrites(0), "fixture confirmation should be granted")
        || !require(manager.setZoneStaticColor(0, 0, scheduledColor), "gated fixture color should apply")
        || !require(manager.saveProfile(QStringLiteral("Gated")), "gated fixture profile should save")
        || !require(manager.setZoneStaticColor(0, 0, currentColor), "gated current color should apply")
        || !require(manager.revokeDeviceWrites(0), "fixture confirmation should revoke")) {
        return 1;
    }

    lumacore::ScheduleService gatedService(&manager);
    lumacore::ScheduleService::Config gatedConfig;
    gatedConfig.enabled = true;
    gatedConfig.profileName = QStringLiteral("Gated");
    gatedConfig.time = QStringLiteral("00:30");
    gatedService.setConfig(gatedConfig);
    gatedService.setClock(clockAt(dayOne, QTime(1, 0)));
    gatedService.evaluateNow();
    gatedService.setClock(clockAt(dayTwo, QTime(0, 31)));
    gatedService.evaluateNow();
    if (!require(
            zoneColor(manager) == currentColor,
            "unconfirmed hardware writes should be refused during scheduled applies"
        )
        || !require(
            logContains(manager, QStringLiteral("requires confirmation")),
            "refused scheduled writes should log the confirmation requirement"
        )
        || !require(
            logContains(
                manager,
                QStringLiteral("Scheduled apply skipped or failed 1 zone(s); unconfirmed hardware requires "
                               "per-session confirmation from the GUI.")
            ),
            "refused scheduled applies should log the follow-up hint"
        )) {
        return 1;
    }

    // Dry-run scheduled applies report success without touching hardware.
    manager.setDryRunEnabled(true);
    lumacore::ScheduleService dryRunService(&manager);
    lumacore::ScheduleService::Config dryRunConfig;
    dryRunConfig.enabled = true;
    dryRunConfig.profileName = QStringLiteral("Gated");
    dryRunConfig.time = QStringLiteral("00:30");
    dryRunService.setConfig(dryRunConfig);
    dryRunService.setClock(clockAt(dayOne, QTime(1, 0)));
    dryRunService.evaluateNow();
    dryRunService.setClock(clockAt(dayTwo, QTime(0, 31)));
    dryRunService.evaluateNow();
    if (!require(zoneColor(manager) == currentColor, "dry-run scheduled applies should not change hardware state")
        || !require(
            logContains(manager, QStringLiteral("[DRY-RUN]")),
            "dry-run scheduled applies should log their write intent"
        )) {
        return 1;
    }

    return 0;
}
