#include "ui/ZoneListModel.h"

namespace lumacore {

ZoneListModel::ZoneListModel(DeviceManager* deviceManager, QObject* parent)
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
        if (deviceIndex != m_deviceIndex || zoneIndex < 0 || zoneIndex >= rowCount()) {
            return;
        }

        const QModelIndex changedIndex = index(zoneIndex, 0);
        emit dataChanged(changedIndex, changedIndex, {ZoneColorRole, ZoneColorHexRole});
    });
}

int ZoneListModel::deviceIndex() const
{
    return m_deviceIndex;
}

void ZoneListModel::setDeviceIndex(int deviceIndex)
{
    if (m_deviceIndex == deviceIndex) {
        return;
    }

    beginResetModel();
    m_deviceIndex = deviceIndex;
    endResetModel();
    emit deviceIndexChanged();
}

int ZoneListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || m_deviceManager == nullptr) {
        return 0;
    }

    const RgbDevice* device = m_deviceManager->deviceAt(m_deviceIndex);
    return device == nullptr ? 0 : static_cast<int>(device->zones().size());
}

QVariant ZoneListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || m_deviceManager == nullptr) {
        return {};
    }

    const RgbDevice* device = m_deviceManager->deviceAt(m_deviceIndex);
    if (device == nullptr || index.row() < 0 || index.row() >= device->zones().size()) {
        return {};
    }

    const RgbZone& zone = device->zones().at(index.row());
    switch (role) {
    case ZoneIndexRole:
        return index.row();
    case ZoneNameRole:
        return zone.name();
    case ZoneTypeRole:
        return zone.typeName();
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

QHash<int, QByteArray> ZoneListModel::roleNames() const
{
    return {
        {ZoneIndexRole, "zoneIndex"},
        {ZoneNameRole, "zoneName"},
        {ZoneTypeRole, "zoneType"},
        {LedCountRole, "ledCount"},
        {ZoneColorRole, "zoneColor"},
        {ZoneColorHexRole, "zoneColorHex"},
    };
}

} // namespace lumacore
