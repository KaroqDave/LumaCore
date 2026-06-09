#pragma once

#include "core/DeviceManager.h"

#include <QAbstractListModel>

namespace lumacore {

class ZoneListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int deviceIndex READ deviceIndex WRITE setDeviceIndex NOTIFY deviceIndexChanged)

public:
    enum Role {
        ZoneIndexRole = Qt::UserRole + 1,
        ZoneNameRole,
        ZoneTypeRole,
        LedCountRole,
        ZoneColorRole,
        ZoneColorHexRole
    };

    explicit ZoneListModel(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] int deviceIndex() const;
    void setDeviceIndex(int deviceIndex);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void deviceIndexChanged();

private:
    DeviceManager* m_deviceManager {nullptr};
    int m_deviceIndex {0};
};

} // namespace lumacore
