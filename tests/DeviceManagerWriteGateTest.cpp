// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/DeviceManager.h"
#include "core/RgbBackend.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

class ConfirmationDevice final : public lumacore::RgbDevice
{
public:
    ConfirmationDevice()
        : RgbDevice(
              QStringLiteral("confirmation-device"),
              QStringLiteral("Confirmation Device"),
              QStringLiteral("Test"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Zone 1"),
            lumacore::RgbZoneType::AddressableHeader,
            4,
            lumacore::RgbColor(1, 2, 3)
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        return applyZoneEffect(zoneIndex, lumacore::RgbEffect(lumacore::RgbEffectType::Static, color));
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneColorWrite
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capability == lumacore::BackendCapability::DiscoveryRead) {
            return {lumacore::PermissionStatus::Granted, {}};
        }

        if (capability == lumacore::BackendCapability::ZoneColorWrite
            || capability == lumacore::BackendCapability::ZoneEffectWrite) {
            return {
                lumacore::PermissionStatus::RequiresConfirmation,
                QStringLiteral("Test writes require confirmation."),
            };
        }

        return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported test operation.")};
    }
};

class ConfirmationBackend final : public lumacore::RgbBackend
{
public:
    [[nodiscard]] lumacore::BackendDescriptor descriptor() const override
    {
        return {
            QStringLiteral("confirmation-test"),
            QStringLiteral("Confirmation Test"),
            QStringLiteral("Confirmation-gated test backend."),
            lumacore::BackendCapability::DiscoveryRead
                | lumacore::BackendCapability::ZoneColorWrite
                | lumacore::BackendCapability::ZoneEffectWrite,
        };
    }

    [[nodiscard]] std::vector<std::unique_ptr<lumacore::RgbDevice>> createDevices() const override
    {
        std::vector<std::unique_ptr<lumacore::RgbDevice>> devices;
        devices.push_back(std::make_unique<ConfirmationDevice>());
        return devices;
    }

    [[nodiscard]] lumacore::PermissionResult probe() const override
    {
        return {lumacore::PermissionStatus::Granted, {}};
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    lumacore::DeviceManager manager;
#ifdef Q_OS_WIN
    if (!require(manager.dryRunEnabled(), "Windows device manager should default to dry-run enabled")) {
        return 1;
    }
#else
    if (!require(!manager.dryRunEnabled(), "non-Windows device manager should default to dry-run disabled")) {
        return 1;
    }
#endif
    manager.setDryRunEnabled(false);
    manager.registerBackend(std::make_unique<ConfirmationBackend>());
    manager.initializeBackends(QStringLiteral("confirmation-test"));

    if (!require(manager.deviceCount() == 1, "confirmation test backend should load one device")) {
        return 1;
    }
    if (!require(!manager.deviceWriteConfirmed(0), "test device should start unconfirmed")) {
        return 1;
    }

    QString errorMessage;
    if (!require(manager.updateZone(0, 0, QStringLiteral("Renamed Zone"), 8, &errorMessage), "zone metadata should update without confirmation")) {
        return 1;
    }
    if (!require(errorMessage.isEmpty(), "successful zone metadata update should not report an error")) {
        return 1;
    }

    const lumacore::RgbDevice* device = manager.deviceAt(0);
    if (!require(device != nullptr, "updated device should still be available")) {
        return 1;
    }
    if (!require(device->zones().at(0).name() == QStringLiteral("Renamed Zone"), "zone name should update")) {
        return 1;
    }
    if (!require(device->zones().at(0).ledCount() == 8, "zone LED count should update")) {
        return 1;
    }

    if (!require(!manager.applyZoneEffect(
              0,
              0,
              lumacore::RgbEffect(lumacore::RgbEffectType::Static, lumacore::RgbColor(20, 40, 80))
          ),
          "effect writes should remain blocked before confirmation")) {
        return 1;
    }
    if (!require(manager.confirmDeviceWrites(0), "test device writes should confirm")) {
        return 1;
    }
    if (!require(manager.applyZoneEffect(
              0,
              0,
              lumacore::RgbEffect(lumacore::RgbEffectType::Static, lumacore::RgbColor(20, 40, 80))
          ),
          "effect writes should apply after confirmation")) {
        return 1;
    }
    manager.paintZoneFrame(0, 0, {lumacore::RgbColor(9, 8, 7)});
    if (!require(
            manager.deviceAt(0)->zones().at(0).currentColor() == lumacore::RgbColor(9, 8, 7),
            "confirmed local animation frames should remain allowed"
        )) {
        return 1;
    }

    return 0;
}
