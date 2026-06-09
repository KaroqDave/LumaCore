#include "ui/DeviceListModel.h"

#include <QColor>

namespace lumacore {

DeviceListModel::DeviceListModel(DeviceManager* deviceManager, QObject* parent)
    : QAbstractListModel(parent)
    , m_deviceManager(deviceManager)
{
    if (m_deviceManager == nullptr) {
        return;
    }

    connect(m_deviceManager, &DeviceManager::devicesChanged, this, [this] {
        beginResetModel();
        endResetModel();
    });

    connect(m_deviceManager, &DeviceManager::zoneColorChanged, this, [this](int deviceIndex, int zoneIndex) {
        const int row = rowForZone(deviceIndex, zoneIndex);
        if (row < 0) {
            return;
        }

        const QModelIndex changedIndex = index(row, 0);
        emit dataChanged(changedIndex, changedIndex, {ZoneColorRole, ZoneColorHexRole});
    });
}

int DeviceListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || m_deviceManager == nullptr) {
        return 0;
    }

    int count = 0;
    for (const std::unique_ptr<RgbDevice>& device : m_deviceManager->devices()) {
        count += static_cast<int>(device->zones().size());
    }

    return count;
}

QVariant DeviceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || m_deviceManager == nullptr) {
        return {};
    }

    const RowRef ref = rowReferenceForRow(index.row());
    const RgbDevice* device = m_deviceManager->deviceAt(ref.deviceIndex);
    if (device == nullptr || ref.zoneIndex < 0 || ref.zoneIndex >= device->zones().size()) {
        return {};
    }

    const RgbZone& zone = device->zones().at(ref.zoneIndex);

    switch (role) {
    case DeviceIndexRole:
        return ref.deviceIndex;
    case ZoneIndexRole:
        return ref.zoneIndex;
    case DeviceNameRole:
        return device->name();
    case ZoneNameRole:
        return zone.name();
    case LedCountRole:
        return zone.ledCount();
    case ZoneColorRole:
        return zone.currentColor().toQColor();
    case ZoneColorHexRole:
        return zone.currentColor().toHexString();
    default:
        return {};
    }
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        {DeviceIndexRole, "deviceIndex"},
        {ZoneIndexRole, "zoneIndex"},
        {DeviceNameRole, "deviceName"},
        {ZoneNameRole, "zoneName"},
        {LedCountRole, "ledCount"},
        {ZoneColorRole, "zoneColor"},
        {ZoneColorHexRole, "zoneColorHex"},
    };
}

bool DeviceListModel::setZoneColor(int row, int red, int green, int blue)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    const RowRef ref = rowReferenceForRow(row);
    if (ref.deviceIndex < 0 || ref.zoneIndex < 0) {
        return false;
    }

    return m_deviceManager->setZoneStaticColor(ref.deviceIndex, ref.zoneIndex, RgbColor::fromRgb(red, green, blue));
}

DeviceListModel::RowRef DeviceListModel::rowReferenceForRow(int row) const
{
    if (row < 0 || m_deviceManager == nullptr) {
        return {};
    }

    int currentRow = 0;
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
        if (device == nullptr) {
            continue;
        }

        const int zoneCount = static_cast<int>(device->zones().size());
        if (row < currentRow + zoneCount) {
            return {deviceIndex, row - currentRow};
        }

        currentRow += zoneCount;
    }

    return {};
}

int DeviceListModel::rowForZone(int deviceIndex, int zoneIndex) const
{
    if (deviceIndex < 0 || zoneIndex < 0 || m_deviceManager == nullptr) {
        return -1;
    }

    int row = 0;
    for (int currentDeviceIndex = 0; currentDeviceIndex < m_deviceManager->deviceCount(); ++currentDeviceIndex) {
        const RgbDevice* device = m_deviceManager->deviceAt(currentDeviceIndex);
        if (device == nullptr) {
            continue;
        }

        if (currentDeviceIndex == deviceIndex) {
            return zoneIndex < device->zones().size() ? row + zoneIndex : -1;
        }

        row += static_cast<int>(device->zones().size());
    }

    return -1;
}

} // namespace lumacore
