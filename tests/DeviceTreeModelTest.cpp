// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/DeviceManager.h"
#include "ui/DeviceTreeModel.h"

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

class TreeFixtureDevice final : public lumacore::RgbDevice
{
public:
    TreeFixtureDevice(QString id, QString name, bool rgbController)
        : RgbDevice(
              std::move(id),
              std::move(name),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        setLikelyRgbController(rgbController);
        setRgbControllerOverride(rgbController);
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Zone"),
            lumacore::RgbZoneType::Motherboard,
            1
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        Q_UNUSED(zoneIndex)
        Q_UNUSED(color)
        return false;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead | lumacore::BackendCapability::ZoneColorWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported capability.")};
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    lumacore::DeviceManager manager;
    std::vector<std::unique_ptr<lumacore::RgbDevice>> devices;
    devices.push_back(std::make_unique<TreeFixtureDevice>(
        QStringLiteral("rgb-device"),
        QStringLiteral("RGB Device"),
        true
    ));
    devices.push_back(std::make_unique<TreeFixtureDevice>(
        QStringLiteral("inventory-device"),
        QStringLiteral("Inventory Device"),
        false
    ));
    manager.replaceDevices(std::move(devices));

    lumacore::DeviceTreeModel treeModel(&manager);

    if (!require(treeModel.rowCount({}) == 2, "default filter should show all devices")) {
        return 1;
    }

    treeModel.setDeviceFilter(lumacore::DeviceTreeModel::RgbControllers);
    if (!require(treeModel.rowCount({}) == 1, "RGB filter should hide non-controller devices")
        || !require(treeModel.isDeviceVisible(0), "RGB device should remain visible")
        || !require(!treeModel.isDeviceVisible(1), "inventory device should be hidden by RGB filter")) {
        return 1;
    }

    const QModelIndex rgbDeviceIndex = treeModel.index(0, 0, {});
    const QString badgeText = treeModel.data(rgbDeviceIndex, lumacore::DeviceTreeModel::DeviceBadgeTextRole).toString();
    if (!require(badgeText == QStringLiteral("Writable"), "writable device should expose a writable badge")) {
        return 1;
    }

    return 0;
}
