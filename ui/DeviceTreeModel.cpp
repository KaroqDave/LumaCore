#include "ui/DeviceTreeModel.h"

#include "core/RgbColor.h"

namespace lumacore {

namespace {

QString effectDisplayName(RgbEffectType type)
{
    switch (type) {
    case RgbEffectType::Static:
        return QStringLiteral("Static");
    case RgbEffectType::Rainbow:
        return QStringLiteral("Rainbow");
    case RgbEffectType::Breathing:
        return QStringLiteral("Breathing");
    case RgbEffectType::ColorCycle:
        return QStringLiteral("Color Cycle");
    }

    return QStringLiteral("Static");
}

bool deviceWritable(const RgbDevice& device)
{
    const BackendCapabilities capabilities = device.capabilities();
    return capabilities.testFlag(BackendCapability::ZoneColorWrite)
        || capabilities.testFlag(BackendCapability::ZoneEffectWrite);
}

QString deviceBadgeText(const RgbDevice& device)
{
    if (deviceWritable(device)) {
        return QStringLiteral("Writable");
    }

    const QString stage = device.discoverySupportStage();
    if (stage == QStringLiteral("guarded-write-backend")) {
        return QStringLiteral("Guarded");
    }
    if (stage == QStringLiteral("research-only")) {
        return QStringLiteral("Research");
    }
    if (stage == QStringLiteral("heuristic")) {
        return QStringLiteral("Discovery");
    }
    if (device.capabilities().testFlag(BackendCapability::DiscoveryRead)) {
        return QStringLiteral("Read-only");
    }

    return {};
}

QString deviceBadgeLevel(const RgbDevice& device)
{
    if (deviceWritable(device) || device.discoverySupportStage() == QStringLiteral("guarded-write-backend")) {
        return QStringLiteral("ready");
    }

    const QString stage = device.discoverySupportStage();
    if (stage == QStringLiteral("research-only") || stage == QStringLiteral("heuristic")) {
        return QStringLiteral("warning");
    }
    if (device.capabilities().testFlag(BackendCapability::DiscoveryRead)) {
        return QStringLiteral("neutral");
    }

    return {};
}

} // namespace

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
            emit dataChanged(changedZone, changedZone, {
                Qt::DisplayRole,
                DisplayNameRole,
                DescriptionRole,
                ZoneTypeRole,
                LedCountRole,
                ZoneColorRole,
                ZoneColorHexRole,
                ZoneEffectTypeRole,
                ZoneEffectNameRole,
                ZoneEffectAnimatedRole,
                ZoneEffectColorHexRole,
            });
        }

        const QModelIndex changedDevice = indexForDevice(deviceIndex);
        if (changedDevice.isValid()) {
            emit dataChanged(changedDevice, changedDevice);
        }
    };

    const auto notifyZoneFrameUpdated = [this](int deviceIndex, int zoneIndex) {
        const QModelIndex changedZone = indexForZone(deviceIndex, zoneIndex);
        if (changedZone.isValid()) {
            emit dataChanged(changedZone, changedZone, {ZoneColorRole, ZoneColorHexRole});
        }
    };

    connect(m_deviceManager, &DeviceManager::zoneChanged, this, notifyZoneChanged);
    connect(m_deviceManager, &DeviceManager::zoneFrameUpdated, this, notifyZoneFrameUpdated);
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
        case IsRgbControllerRole:
            return device->isRgbController();
        case HasRgbControllerOverrideRole:
            return device->hasRgbControllerOverride();
        case RgbControllerOverrideRole:
            return device->rgbControllerOverride();
        case DeviceWritableRole:
            return deviceWritable(*device);
        case DeviceBadgeTextRole:
            return deviceBadgeText(*device);
        case DeviceBadgeLevelRole:
            return deviceBadgeLevel(*device);
        case DiscoverySupportStageRole:
            return device->discoverySupportStage();
        case DiscoverySupportStatusRole:
            return device->discoverySupportStatus();
        case DiscoverySupportFamilyRole:
            return device->discoverySupportFamily();
        default:
            return {};
        }
    }

    if (node->zoneIndex < 0 || node->zoneIndex >= device->zones().size()) {
        return {};
    }

    const RgbZone& zone = device->zones().at(node->zoneIndex);
    const RgbEffect& effect = zone.effect();
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
    case ZoneEffectTypeRole:
        return static_cast<int>(effect.type());
    case ZoneEffectNameRole:
        return effectDisplayName(effect.type());
    case ZoneEffectAnimatedRole:
        return effect.isAnimated();
    case ZoneEffectColorHexRole:
        return effect.color().toHexString();
    case ZoneCountRole:
        return 0;
    case IsDeviceRole:
        return false;
    case IsZoneRole:
        return true;
    case IsRgbControllerRole:
        return device->isRgbController();
    case HasRgbControllerOverrideRole:
        return device->hasRgbControllerOverride();
    case RgbControllerOverrideRole:
        return device->rgbControllerOverride();
    case DeviceWritableRole:
        return deviceWritable(*device);
    case DeviceBadgeTextRole:
        return deviceBadgeText(*device);
    case DeviceBadgeLevelRole:
        return deviceBadgeLevel(*device);
    case DiscoverySupportStageRole:
        return device->discoverySupportStage();
    case DiscoverySupportStatusRole:
        return device->discoverySupportStatus();
    case DiscoverySupportFamilyRole:
        return device->discoverySupportFamily();
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
        {ZoneEffectTypeRole, "zoneEffectType"},
        {ZoneEffectNameRole, "zoneEffectName"},
        {ZoneEffectAnimatedRole, "zoneEffectAnimated"},
        {ZoneEffectColorHexRole, "zoneEffectColorHex"},
        {ZoneCountRole, "zoneCount"},
        {IsDeviceRole, "isDevice"},
        {IsZoneRole, "isZone"},
        {IsRgbControllerRole, "isRgbController"},
        {HasRgbControllerOverrideRole, "hasRgbControllerOverride"},
        {RgbControllerOverrideRole, "rgbControllerOverride"},
        {DeviceWritableRole, "deviceWritable"},
        {DeviceBadgeTextRole, "deviceBadgeText"},
        {DeviceBadgeLevelRole, "deviceBadgeLevel"},
        {DiscoverySupportStageRole, "discoverySupportStage"},
        {DiscoverySupportStatusRole, "discoverySupportStatus"},
        {DiscoverySupportFamilyRole, "discoverySupportFamily"},
    };
}

int DeviceTreeModel::deviceFilter() const
{
    return static_cast<int>(m_deviceFilter);
}

void DeviceTreeModel::setDeviceFilter(int deviceFilter)
{
    const DeviceFilter nextFilter = deviceFilter == RgbControllers ? RgbControllers : AllDevices;
    if (m_deviceFilter == nextFilter) {
        return;
    }

    beginResetModel();
    m_deviceFilter = nextFilter;
    rebuild();
    endResetModel();
    emit deviceFilterChanged();
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

        if (m_deviceFilter == RgbControllers && !device->isRgbController()) {
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

QModelIndex DeviceTreeModel::indexForDevice(int deviceIndex) const
{
    if (m_root == nullptr || deviceIndex < 0) {
        return {};
    }

    for (int row = 0; row < static_cast<int>(m_root->children.size()); ++row) {
        const TreeNode* deviceNode = m_root->children.at(static_cast<std::size_t>(row)).get();
        if (deviceNode != nullptr && deviceNode->deviceIndex == deviceIndex) {
            return createIndex(row, 0, const_cast<TreeNode*>(deviceNode));
        }
    }

    return {};
}

QModelIndex DeviceTreeModel::indexForZone(int deviceIndex, int zoneIndex) const
{
    if (m_root == nullptr || deviceIndex < 0) {
        return {};
    }

    const QModelIndex deviceModelIndex = indexForDevice(deviceIndex);
    const TreeNode* deviceNode = nodeFromIndex(deviceModelIndex);
    if (deviceNode == nullptr || zoneIndex < 0 || zoneIndex >= static_cast<int>(deviceNode->children.size())) {
        return {};
    }

    return createIndex(zoneIndex, 0, deviceNode->children.at(static_cast<std::size_t>(zoneIndex)).get());
}

} // namespace lumacore
