// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockBackend.h"
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

    const QModelIndex rgbZoneIndex = treeModel.index(0, 0, rgbDeviceIndex);
    if (!require(rgbZoneIndex.isValid(), "rgb device should expose a zone row")
        || !require(
            !treeModel.data(rgbZoneIndex, lumacore::DeviceTreeModel::ZoneStreamingRole).toBool(),
            "zones should not report frame streaming before any effect streams"
        )) {
        return 1;
    }
    manager.startZoneFrameStreaming(0, 0);
    if (!require(
            treeModel.data(rgbZoneIndex, lumacore::DeviceTreeModel::ZoneStreamingRole).toBool(),
            "zones should report frame streaming while the effects engine streams them"
        )) {
        return 1;
    }
    manager.stopZoneFrameStreaming(0, 0);
    if (!require(
            !treeModel.data(rgbZoneIndex, lumacore::DeviceTreeModel::ZoneStreamingRole).toBool(),
            "zones should stop reporting frame streaming once streaming stops"
        )) {
        return 1;
    }

    {
        lumacore::DeviceManager manyZoneManager;
        manyZoneManager.registerBackend(std::make_unique<lumacore::MockBackend>(QStringLiteral("many-zones")));
        manyZoneManager.initializeBackends(QStringLiteral("mock"));
        lumacore::DeviceTreeModel manyZoneModel(&manyZoneManager);
        if (!require(manyZoneModel.rowCount({}) == 1, "many-zones scenario should expose one device row")) {
            return 1;
        }

        const QModelIndex deviceIndex = manyZoneModel.index(0, 0, {});
        if (!require(deviceIndex.isValid(), "many-zones device index should be valid")
            || !require(
                manyZoneModel.data(deviceIndex, lumacore::DeviceTreeModel::DisplayNameRole).toString()
                    == QStringLiteral("Mock Many-Zone Controller"),
                "many-zones device row should expose the mock device name"
            )
            || !require(
                manyZoneModel.data(deviceIndex, lumacore::DeviceTreeModel::ZoneCountRole).toInt() == 16,
                "many-zones device row should expose sixteen child zones"
            )
            || !require(
                manyZoneModel.rowCount(deviceIndex) == 16,
                "many-zones device row should have sixteen child rows"
            )
            || !require(
                manyZoneModel.data(deviceIndex, lumacore::DeviceTreeModel::DeviceBadgeTextRole).toString()
                    == QStringLiteral("Writable"),
                "many-zones device row should remain writable"
            )) {
            return 1;
        }

        const QModelIndex firstZone = manyZoneModel.index(0, 0, deviceIndex);
        const QModelIndex tenthZone = manyZoneModel.index(9, 0, deviceIndex);
        const QModelIndex lastZone = manyZoneModel.index(15, 0, deviceIndex);
        if (!require(firstZone.isValid(), "first many-zones child should be valid")
            || !require(tenthZone.isValid(), "tenth many-zones child should be valid")
            || !require(lastZone.isValid(), "last many-zones child should be valid")
            || !require(
                manyZoneModel.parent(firstZone) == deviceIndex,
                "many-zones first child should parent back to the device"
            )
            || !require(
                manyZoneModel.data(firstZone, lumacore::DeviceTreeModel::DisplayNameRole).toString()
                    == QStringLiteral("Zone 01"),
                "many-zones first child should expose the padded first zone name"
            )
            || !require(
                manyZoneModel.data(tenthZone, lumacore::DeviceTreeModel::DisplayNameRole).toString()
                    == QStringLiteral("Zone 10"),
                "many-zones tenth child should expose a double-digit zone name"
            )
            || !require(
                manyZoneModel.data(lastZone, lumacore::DeviceTreeModel::DisplayNameRole).toString()
                    == QStringLiteral("Zone 16"),
                "many-zones last child should expose the final zone name"
            )
            || !require(
                manyZoneModel.data(lastZone, lumacore::DeviceTreeModel::ZoneIndexRole).toInt() == 15,
                "many-zones last child should preserve the zero-based zone index"
            )
            || !require(
                manyZoneModel.data(lastZone, lumacore::DeviceTreeModel::LedCountRole).toInt() == 36,
                "many-zones last child should expose the addressable stress LED count"
            )) {
            return 1;
        }

        manyZoneModel.setDeviceFilter(lumacore::DeviceTreeModel::RgbControllers);
        if (!require(manyZoneModel.rowCount({}) == 1, "RGB filter should keep the many-zones controller visible")
            || !require(manyZoneModel.isDeviceVisible(0), "many-zones controller should be visible under the RGB filter")) {
            return 1;
        }
    }

    return 0;
}
