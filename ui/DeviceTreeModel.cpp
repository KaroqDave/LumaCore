#include "ui/DeviceTreeModel.h"

#include "core/RgbColor.h"

namespace lumacore {

DeviceTreeModel::DeviceTreeModel(DeviceManager* deviceManager, QObject* parent)
    : QAbstractItemModel(parent)
    , m_deviceManager(deviceManager)
{
    rebuild();

    if (m_deviceManager == nullptr) {
        return;
    }

    connect(m_deviceManager, &DeviceManager::devicesChanged, this, [this] {
        beginResetModel();
        rebuild();
        endResetModel();
    });

    const auto notifyZoneChanged = [this](int deviceIndex, int zoneIndex) {
        const QModelIndex changedZone = indexForZone(deviceIndex, zoneIndex);
        if (changedZone.isValid()) {
            emit dataChanged(changedZone, changedZone);
        }

        const QModelIndex changedDevice = index(deviceIndex, 0);
        if (changedDevice.isValid()) {
            emit dataChanged(changedDevice, changedDevice);
        }
    };

    connect(m_deviceManager, &DeviceManager::zoneChanged, this, notifyZoneChanged);
    connect(m_deviceManager, &DeviceManager::zoneFrameUpdated, this, notifyZoneChanged);
}

QModelIndex DeviceTreeModel::index(int row, int column, const QModelIndex& parentIndex) const
{
    if (column != 0 || row < 0) {
        return {};
    }

    const TreeNode* parentNode = nodeFromIndex(parentIndex);
    if (parentNode == nullptr || row >= static_cast<int>(parentNode->children.size())) {
        return {};
    }

    return createIndex(row, column, parentNode->children.at(static_cast<std::size_t>(row)).get());
}

QModelIndex DeviceTreeModel::parent(const QModelIndex& child) const
{
    const TreeNode* childNode = nodeFromIndex(child);
    if (childNode == nullptr || childNode->parent == nullptr || childNode->parent == m_root.get()) {
        return {};
    }

    return createIndex(childNode->parent->row, 0, childNode->parent);
}

int DeviceTreeModel::rowCount(const QModelIndex& parentIndex) const
{
    if (parentIndex.column() > 0) {
        return 0;
    }

    const TreeNode* parentNode = nodeFromIndex(parentIndex);
    return parentNode == nullptr ? 0 : static_cast<int>(parentNode->children.size());
}

int DeviceTreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant DeviceTreeModel::data(const QModelIndex& index, int role) const
{
    const TreeNode* node = nodeFromIndex(index);
    if (node == nullptr || node->kind == NodeKind::Root || m_deviceManager == nullptr) {
        return {};
    }

    const RgbDevice* device = m_deviceManager->deviceAt(node->deviceIndex);
    if (device == nullptr) {
        return {};
    }

    if (node->kind == NodeKind::Device) {
        switch (role) {
        case Qt::DisplayRole:
        case DisplayNameRole:
            return device->name();
        case NodeTypeRole:
            return QStringLiteral("device");
        case DescriptionRole:
            return QStringLiteral("%1 · %2 zone(s)").arg(device->vendor()).arg(device->zones().size());
        case DeviceIndexRole:
            return node->deviceIndex;
        case ZoneIndexRole:
            return -1;
        case ZoneCountRole:
            return device->zones().size();
        case IsDeviceRole:
            return true;
        case IsZoneRole:
            return false;
        default:
            return {};
        }
    }

    if (node->zoneIndex < 0 || node->zoneIndex >= device->zones().size()) {
        return {};
    }

    const RgbZone& zone = device->zones().at(node->zoneIndex);
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return zone.name();
    case NodeTypeRole:
        return QStringLiteral("zone");
    case DescriptionRole:
        return QStringLiteral("%1 · %2 LED(s)").arg(zone.typeName()).arg(zone.ledCount());
    case DeviceIndexRole:
        return node->deviceIndex;
    case ZoneIndexRole:
        return node->zoneIndex;
    case ZoneTypeRole:
        return zone.typeName();
    case LedCountRole:
        return zone.ledCount();
    case ZoneColorRole:
        return zone.currentColor().toQColor();
    case ZoneColorHexRole:
        return zone.currentColor().toHexString();
    case ZoneCountRole:
        return 0;
    case IsDeviceRole:
        return false;
    case IsZoneRole:
        return true;
    default:
        return {};
    }
}

Qt::ItemFlags DeviceTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> DeviceTreeModel::roleNames() const
{
    return {
        {NodeTypeRole, "nodeType"},
        {DisplayNameRole, "displayName"},
        {DescriptionRole, "description"},
        {DeviceIndexRole, "deviceIndex"},
        {ZoneIndexRole, "zoneIndex"},
        {ZoneTypeRole, "zoneType"},
        {LedCountRole, "ledCount"},
        {ZoneColorRole, "zoneColor"},
        {ZoneColorHexRole, "zoneColorHex"},
        {ZoneCountRole, "zoneCount"},
        {IsDeviceRole, "isDevice"},
        {IsZoneRole, "isZone"},
    };
}

void DeviceTreeModel::rebuild()
{
    m_root = std::make_unique<TreeNode>();

    if (m_deviceManager == nullptr) {
        return;
    }

    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
        if (device == nullptr) {
            continue;
        }

        auto deviceNode = std::make_unique<TreeNode>();
        deviceNode->kind = NodeKind::Device;
        deviceNode->parent = m_root.get();
        deviceNode->row = static_cast<int>(m_root->children.size());
        deviceNode->deviceIndex = deviceIndex;

        for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
            auto zoneNode = std::make_unique<TreeNode>();
            zoneNode->kind = NodeKind::Zone;
            zoneNode->parent = deviceNode.get();
            zoneNode->row = static_cast<int>(deviceNode->children.size());
            zoneNode->deviceIndex = deviceIndex;
            zoneNode->zoneIndex = zoneIndex;
            deviceNode->children.push_back(std::move(zoneNode));
        }

        m_root->children.push_back(std::move(deviceNode));
    }
}

DeviceTreeModel::TreeNode* DeviceTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (index.isValid()) {
        return static_cast<TreeNode*>(index.internalPointer());
    }

    return m_root.get();
}

QModelIndex DeviceTreeModel::indexForZone(int deviceIndex, int zoneIndex) const
{
    if (m_root == nullptr || deviceIndex < 0 || deviceIndex >= static_cast<int>(m_root->children.size())) {
        return {};
    }

    const TreeNode* deviceNode = m_root->children.at(static_cast<std::size_t>(deviceIndex)).get();
    if (deviceNode == nullptr || zoneIndex < 0 || zoneIndex >= static_cast<int>(deviceNode->children.size())) {
        return {};
    }

    return createIndex(zoneIndex, 0, deviceNode->children.at(static_cast<std::size_t>(zoneIndex)).get());
}

} // namespace lumacore
