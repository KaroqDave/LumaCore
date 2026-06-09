#include "backends/mock/MockBackend.h"

#include "backends/mock/MockRgbDevice.h"

namespace lumacore {

std::vector<std::unique_ptr<RgbDevice>> MockBackend::createDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    devices.push_back(std::make_unique<MockRgbDevice>());
    return devices;
}

} // namespace lumacore
