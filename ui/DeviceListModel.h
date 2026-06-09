#pragma once

#include "core/DeviceManager.h"

#include <QAbstractListModel>

namespace lumacore {

class DeviceListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        DeviceIndexRole = Qt::UserRole + 1,
        ZoneIndexRole,
        DeviceNameRole,
        ZoneNameRole,
        LedCountRole,
        ZoneColorRole,
        ZoneColorHexRole
    };

    explicit DeviceListModel(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool setZoneColor(int row, int red, int green, int blue);

private:
    struct RowRef {
        int deviceIndex {-1};
        int zoneIndex {-1};
    };

    [[nodiscard]] RowRef rowReferenceForRow(int row) const;
    [[nodiscard]] int rowForZone(int deviceIndex, int zoneIndex) const;

    DeviceManager* m_deviceManager {nullptr};
};

} // namespace lumacore
