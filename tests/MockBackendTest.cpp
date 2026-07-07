// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockBackend.h"
#include "core/PermissionGate.h"
#include "core/RgbDevice.h"

#include <QCoreApplication>
#include <QDebug>

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

std::unique_ptr<lumacore::RgbDevice> discoverSingleDevice(const QString& scenarioId)
{
    lumacore::MockBackend backend(scenarioId);
    std::vector<std::unique_ptr<lumacore::RgbDevice>> devices = backend.discoverDevices();
    if (devices.empty()) {
        return {};
    }
    return std::move(devices.front());
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace lumacore;

    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    if (!require(
            MockBackend::scenarioIds().contains(MockBackend::defaultScenarioId()),
            "mock backend should list its default scenario"
        )
        || !require(
            MockBackend::isKnownScenarioId(QStringLiteral("many-zones")),
            "mock backend should recognize documented scenario ids"
        )
        || !require(
            !MockBackend::isKnownScenarioId(QStringLiteral("typo")),
            "mock backend should reject unknown scenario ids"
        )) {
        return 1;
    }

    const std::unique_ptr<RgbDevice> defaultDevice = discoverSingleDevice(QStringLiteral("asus-board"));
    if (!require(defaultDevice != nullptr, "default mock scenario should create a device")
        || !require(
            defaultDevice->id() == QStringLiteral("mock-asus-tuf-x870-plus-wifi"),
            "default mock scenario should preserve the stable ASUS-like id"
        )
        || !require(defaultDevice->zones().size() == 3, "default mock scenario should preserve three zones")
        || !require(
            defaultDevice->capabilities().testFlag(BackendCapability::ZoneEffectWrite),
            "default mock scenario should remain writable"
        )) {
        return 1;
    }

    const std::unique_ptr<RgbDevice> readOnlyDevice = discoverSingleDevice(QStringLiteral("read-only"));
    if (!require(readOnlyDevice != nullptr, "read-only mock scenario should create a device")
        || !require(
            readOnlyDevice->capabilities().testFlag(BackendCapability::DiscoveryRead)
                && !readOnlyDevice->capabilities().testFlag(BackendCapability::ZoneColorWrite)
                && !readOnlyDevice->capabilities().testFlag(BackendCapability::ZoneEffectWrite),
            "read-only mock scenario should expose discovery only"
        )
        || !require(
            PermissionGate::checkAnyWrite(*readOnlyDevice).status == PermissionStatus::Denied,
            "read-only mock scenario should deny aggregate writes"
        )) {
        return 1;
    }

    const std::unique_ptr<RgbDevice> confirmationDevice = discoverSingleDevice(QStringLiteral("confirmation-required"));
    if (!require(confirmationDevice != nullptr, "confirmation-required mock scenario should create a device")
        || !require(
            PermissionGate::checkAnyWrite(*confirmationDevice).status == PermissionStatus::RequiresConfirmation,
            "confirmation-required mock scenario should require write confirmation"
        )
        || !require(
            PermissionGate::writeRequiresConfirmation(*confirmationDevice),
            "confirmation-required mock scenario should trip the confirmation helper"
        )) {
        return 1;
    }

    std::unique_ptr<RgbDevice> failingDevice = discoverSingleDevice(QStringLiteral("failing-writes"));
    if (!require(failingDevice != nullptr, "failing-writes mock scenario should create a device")
        || !require(
            !failingDevice->applyZoneEffect(0, RgbEffect(RgbEffectType::Static, RgbColor(1, 2, 3))),
            "failing-writes mock scenario should reject effect writes"
        )
        || !require(
            failingDevice->lastHardwareWriteStatus().contains(QStringLiteral("failing-writes")),
            "failing-writes mock scenario should explain the simulated failure"
        )) {
        return 1;
    }

    const std::unique_ptr<RgbDevice> manyZonesDevice = discoverSingleDevice(QStringLiteral("many-zones"));
    if (!require(manyZonesDevice != nullptr, "many-zones mock scenario should create a device")
        || !require(manyZonesDevice->zones().size() == 16, "many-zones mock scenario should expose sixteen zones")
        || !require(
            manyZonesDevice->zones().first().name() == QStringLiteral("Zone 01"),
            "many-zones mock scenario should expose stable padded zone names"
        )
        || !require(
            manyZonesDevice->zones().last().ledCount() == 36,
            "many-zones mock scenario should include addressable stress zones"
        )) {
        return 1;
    }

    return 0;
}
