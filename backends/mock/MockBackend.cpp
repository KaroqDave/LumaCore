#include "backends/mock/MockBackend.h"

#include "backends/mock/MockRgbDevice.h"

namespace lumacore {

BackendDescriptor MockBackend::descriptor() const
{
    return {
        QStringLiteral("mock"),
        QStringLiteral("Mock Backend"),
        QStringLiteral("In-memory ASUS motherboard simulation with no hardware access."),
        BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite,
    };
}

std::vector<std::unique_ptr<RgbDevice>> MockBackend::createDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    devices.push_back(std::make_unique<MockRgbDevice>());
    return devices;
}

std::vector<std::unique_ptr<RgbDevice>> MockBackend::discoverDevices() const
{
    return createDevices();
}

PermissionResult MockBackend::probe() const
{
    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
