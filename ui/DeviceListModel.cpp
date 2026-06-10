#include "ui/DeviceListModel.h"

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

    connect(m_deviceManager, &DeviceManager::zoneChanged, this, [this](int deviceIndex, int) {
        const QModelIndex changedIndex = index(deviceIndex, 0);
        emit dataChanged(changedIndex, changedIndex, {ZoneCountRole});
    });
}

int DeviceListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || m_deviceManager == nullptr) {
        return 0;
    }

    return m_deviceManager->deviceCount();
}

QVariant DeviceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || m_deviceManager == nullptr) {
        return {};
    }

    const RgbDevice* device = m_deviceManager->deviceAt(index.row());
    if (device == nullptr) {
        return {};
    }

    switch (role) {
    case DeviceIndexRole:
        return index.row();
    case DeviceNameRole:
        return device->name();
    case VendorRole:
        return device->vendor();
    case DeviceTypeRole:
        return device->typeName();
    case ZoneCountRole:
        return device->zones().size();
    default:
        return {};
    }
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        {DeviceIndexRole, "deviceIndex"},
        {DeviceNameRole, "deviceName"},
        {VendorRole, "vendor"},
        {DeviceTypeRole, "deviceType"},
        {ZoneCountRole, "zoneCount"},
    };
}

} // namespace lumacore
