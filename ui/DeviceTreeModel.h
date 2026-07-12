// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/DeviceManager.h"

#include <QAbstractItemModel>

#include <memory>
#include <vector>

namespace lumacore {

class AppController;

class DeviceTreeModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(int deviceFilter READ deviceFilter WRITE setDeviceFilter NOTIFY deviceFilterChanged)

public:
    enum DeviceFilter {
        AllDevices = 0,
        RgbControllers = 1,
    };
    Q_ENUM(DeviceFilter)

    enum Role {
        NodeTypeRole = Qt::UserRole + 1,
        DisplayNameRole,
        DescriptionRole,
        DeviceIndexRole,
        ZoneIndexRole,
        ZoneTypeRole,
        LedCountRole,
        ZoneColorRole,
        ZoneColorHexRole,
        ZoneEffectTypeRole,
        ZoneEffectNameRole,
        ZoneEffectAnimatedRole,
        ZoneEffectColorHexRole,
        ZoneCountRole,
        IsDeviceRole,
        IsZoneRole,
        IsRgbControllerRole,
        HasRgbControllerOverrideRole,
        RgbControllerOverrideRole,
        DeviceWritableRole,
        DeviceBadgeTextRole,
        DeviceBadgeLevelRole,
        DiscoverySupportStageRole,
        DiscoverySupportStatusRole,
        DiscoverySupportFamilyRole,
        ZoneStreamingRole
    };

    explicit DeviceTreeModel(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int deviceFilter() const;
    void setDeviceFilter(int deviceFilter);
    void setWriteConfirmationSource(AppController* controller);
    [[nodiscard]] Q_INVOKABLE bool isDeviceVisible(int deviceIndex) const;

signals:
    void deviceFilterChanged();

private:
    enum class NodeKind {
        Root,
        Device,
        Zone
    };

    struct TreeNode {
        NodeKind kind {NodeKind::Root};
        TreeNode* parent {nullptr};
        std::vector<std::unique_ptr<TreeNode>> children;
        int row {0};
        int deviceIndex {-1};
        int zoneIndex {-1};
    };

    void rebuild();
    [[nodiscard]] TreeNode* nodeFromIndex(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexForDevice(int deviceIndex) const;
    [[nodiscard]] QModelIndex indexForZone(int deviceIndex, int zoneIndex) const;

    void refreshDeviceBadges(int deviceIndex);

    DeviceManager* m_deviceManager {nullptr};
    AppController* m_writeConfirmationSource {nullptr};
    std::unique_ptr<TreeNode> m_root;
    DeviceFilter m_deviceFilter {AllDevices};
};

} // namespace lumacore
