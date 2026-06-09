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
        DeviceNameRole,
        VendorRole,
        DeviceTypeRole,
        ZoneCountRole
    };

    explicit DeviceListModel(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    DeviceManager* m_deviceManager {nullptr};
};

} // namespace lumacore
