#include "core/DeviceManager.h"

#include "backends/mock/MockRgbDevice.h"

namespace lumacore {

DeviceManager::DeviceManager(QObject* parent)
    : QObject(parent)
{
}

void DeviceManager::initializeMockDevices()
{
    m_devices.clear();
    registerDevice(std::make_unique<MockRgbDevice>());
    emit devicesChanged();
}

int DeviceManager::deviceCount() const
{
    return static_cast<int>(m_devices.size());
}

RgbDevice* DeviceManager::deviceAt(int index)
{
    if (index < 0 || index >= deviceCount()) {
        return nullptr;
    }

    return m_devices.at(static_cast<std::size_t>(index)).get();
}

const RgbDevice* DeviceManager::deviceAt(int index) const
{
    if (index < 0 || index >= deviceCount()) {
        return nullptr;
    }

    return m_devices.at(static_cast<std::size_t>(index)).get();
}

const std::vector<std::unique_ptr<RgbDevice>>& DeviceManager::devices() const
{
    return m_devices;
}

bool DeviceManager::setZoneStaticColor(int deviceIndex, int zoneIndex, const RgbColor& color)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return device->setZoneStaticColor(zoneIndex, color);
}

void DeviceManager::registerDevice(std::unique_ptr<RgbDevice> device)
{
    if (!device) {
        return;
    }

    const int deviceIndex = deviceCount();
    connect(device.get(), &RgbDevice::zoneChanged, this, [this, deviceIndex](int zoneIndex) {
        emit zoneColorChanged(deviceIndex, zoneIndex);
    });

    m_devices.push_back(std::move(device));
}

} // namespace lumacore
