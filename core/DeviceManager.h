#pragma once

#include "core/RgbColor.h"
#include "core/RgbDevice.h"

#include <QObject>

#include <memory>
#include <vector>

namespace lumacore {

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject* parent = nullptr);

    void initializeMockDevices();

    [[nodiscard]] int deviceCount() const;
    [[nodiscard]] RgbDevice* deviceAt(int index);
    [[nodiscard]] const RgbDevice* deviceAt(int index) const;
    [[nodiscard]] const std::vector<std::unique_ptr<RgbDevice>>& devices() const;

    [[nodiscard]] bool setZoneStaticColor(int deviceIndex, int zoneIndex, const RgbColor& color);

signals:
    void devicesChanged();
    void zoneColorChanged(int deviceIndex, int zoneIndex);

private:
    void registerDevice(std::unique_ptr<RgbDevice> device);

    std::vector<std::unique_ptr<RgbDevice>> m_devices;
};

} // namespace lumacore
