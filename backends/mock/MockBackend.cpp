#include "backends/mock/MockBackend.h"

#include "backends/mock/MockRgbDevice.h"

#include <QtGlobal>

namespace lumacore {

BackendDescriptor MockBackend::descriptor() const
{
    return {
        QStringLiteral("mock"),
        QStringLiteral("Mock Backend"),
#ifdef Q_OS_WIN
        QStringLiteral("Windows Preview demo backend. Simulated devices only; no hardware discovery or RGB writes are performed."),
#else
        QStringLiteral("In-memory ASUS motherboard simulation with no hardware access."),
#endif
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
