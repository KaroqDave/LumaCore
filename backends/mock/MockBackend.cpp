// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockBackend.h"

#include "backends/mock/MockRgbDevice.h"

#include <QtGlobal>

namespace lumacore {

namespace {

constexpr auto kAsusBoardScenario = "asus-board";
constexpr auto kReadOnlyScenario = "read-only";
constexpr auto kConfirmationRequiredScenario = "confirmation-required";
constexpr auto kFailingWritesScenario = "failing-writes";
constexpr auto kManyZonesScenario = "many-zones";

MockRgbDeviceConfig readOnlyConfig()
{
    MockRgbDeviceConfig config;
    config.id = QStringLiteral("mock-read-only-inventory");
    config.name = QStringLiteral("Mock Read-Only RGB Inventory");
    config.vendor = QStringLiteral("Generic");
    config.type = RgbDeviceType::Controller;
    config.capabilities = BackendCapability::DiscoveryRead;
    // No write capabilities, so checkRuntimePermission always returns the
    // "does not support" Denied result before runtimeWritePermission is read;
    // the field is intentionally left at its struct default here.
    config.likelyRgbController = true;
    config.zones.append(RgbZone(
        QStringLiteral("Read-Only Strip"),
        RgbZoneType::AddressableHeader,
        24,
        RgbColor(95, 160, 255)
    ));
    config.zones.append(RgbZone(
        QStringLiteral("Read-Only Logo"),
        RgbZoneType::Motherboard,
        4,
        RgbColor(255, 196, 87)
    ));
    return config;
}

MockRgbDeviceConfig confirmationRequiredConfig()
{
    MockRgbDeviceConfig config;
    config.id = QStringLiteral("mock-confirmation-required-controller");
    config.name = QStringLiteral("Mock Confirmation Required Controller");
    config.vendor = QStringLiteral("LumaCore");
    config.type = RgbDeviceType::Controller;
    config.capabilities = BackendCapability::DiscoveryRead
        | BackendCapability::ZoneColorWrite
        | BackendCapability::ZoneEffectWrite;
    config.runtimeWritePermission = {
        PermissionStatus::RequiresConfirmation,
        QStringLiteral("Mock scenario requires per-session confirmation before writes."),
    };
    config.likelyRgbController = true;
    config.zones.append(RgbZone(
        QStringLiteral("Guarded Zone"),
        RgbZoneType::AddressableHeader,
        18,
        RgbColor(255, 99, 132)
    ));
    return config;
}

MockRgbDeviceConfig failingWritesConfig()
{
    MockRgbDeviceConfig config = confirmationRequiredConfig();
    config.id = QStringLiteral("mock-failing-writes-controller");
    config.name = QStringLiteral("Mock Failing Writes Controller");
    config.runtimeWritePermission = {PermissionStatus::Granted, {}};
    config.failWrites = true;
    return config;
}

MockRgbDeviceConfig manyZonesConfig()
{
    MockRgbDeviceConfig config;
    config.id = QStringLiteral("mock-many-zone-controller");
    config.name = QStringLiteral("Mock Many-Zone Controller");
    config.vendor = QStringLiteral("LumaCore");
    config.type = RgbDeviceType::Controller;
    config.capabilities = BackendCapability::DiscoveryRead
        | BackendCapability::ZoneColorWrite
        | BackendCapability::ZoneEffectWrite;
    config.runtimeWritePermission = {PermissionStatus::Granted, {}};
    config.likelyRgbController = true;

    for (int zoneIndex = 0; zoneIndex < 16; ++zoneIndex) {
        const int displayIndex = zoneIndex + 1;
        const RgbZoneType type = zoneIndex % 4 == 0
            ? RgbZoneType::Motherboard
            : RgbZoneType::AddressableHeader;
        const int ledCount = type == RgbZoneType::Motherboard ? 6 : 36;
        config.zones.append(RgbZone(
            QStringLiteral("Zone %1").arg(displayIndex, 2, 10, QLatin1Char('0')),
            type,
            ledCount,
            RgbColor(
                (40 + zoneIndex * 13) % 256,
                (140 + zoneIndex * 17) % 256,
                (220 + zoneIndex * 19) % 256
            )
        ));
    }
    return config;
}

MockRgbDeviceConfig scenarioConfig(const QString& scenarioId)
{
    if (scenarioId == QString::fromLatin1(kReadOnlyScenario)) {
        return readOnlyConfig();
    }
    if (scenarioId == QString::fromLatin1(kConfirmationRequiredScenario)) {
        return confirmationRequiredConfig();
    }
    if (scenarioId == QString::fromLatin1(kFailingWritesScenario)) {
        return failingWritesConfig();
    }
    if (scenarioId == QString::fromLatin1(kManyZonesScenario)) {
        return manyZonesConfig();
    }
    Q_UNREACHABLE();
    return readOnlyConfig();
}

} // namespace

MockBackend::MockBackend(QString scenarioId)
    : m_scenarioId(
          scenarioId.trimmed().isEmpty() || !isKnownScenarioId(scenarioId)
              ? defaultScenarioId()
              : scenarioId.trimmed()
      )
{
}

QString MockBackend::defaultScenarioId()
{
    return QString::fromLatin1(kAsusBoardScenario);
}

QStringList MockBackend::scenarioIds()
{
    return {
        QString::fromLatin1(kAsusBoardScenario),
        QString::fromLatin1(kReadOnlyScenario),
        QString::fromLatin1(kConfirmationRequiredScenario),
        QString::fromLatin1(kFailingWritesScenario),
        QString::fromLatin1(kManyZonesScenario),
    };
}

bool MockBackend::isKnownScenarioId(const QString& scenarioId)
{
    const QString normalized = scenarioId.trimmed();
    return normalized.isEmpty() || scenarioIds().contains(normalized);
}

BackendDescriptor MockBackend::descriptor() const
{
    return {
        QStringLiteral("mock"),
        QStringLiteral("Mock Backend"),
#ifdef Q_OS_WIN
        QStringLiteral("Simulated backend with in-memory RGB devices for development and tests."),
#else
        QStringLiteral("In-memory ASUS motherboard simulation with no hardware access."),
#endif
        BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite,
    };
}

std::vector<std::unique_ptr<RgbDevice>> MockBackend::discoverDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    if (m_scenarioId == defaultScenarioId()) {
        devices.push_back(std::make_unique<MockRgbDevice>());
    } else {
        devices.push_back(std::make_unique<MockRgbDevice>(scenarioConfig(m_scenarioId)));
    }
    return devices;
}

PermissionResult MockBackend::probe() const
{
    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
