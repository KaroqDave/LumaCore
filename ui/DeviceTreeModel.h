#pragma once

#include "core/DeviceManager.h"

#include <QAbstractItemModel>

#include <memory>
#include <vector>

namespace lumacore {

class DeviceTreeModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
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
        ZoneCountRole,
        IsDeviceRole,
        IsZoneRole
    };

    explicit DeviceTreeModel(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

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
    [[nodiscard]] QModelIndex indexForZone(int deviceIndex, int zoneIndex) const;

    DeviceManager* m_deviceManager {nullptr};
    std::unique_ptr<TreeNode> m_root;
};

} // namespace lumacore
