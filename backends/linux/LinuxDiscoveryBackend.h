#pragma once

#include "core/RgbBackend.h"

namespace lumacore {

class LinuxDiscoveryBackend final : public RgbBackend
{
public:
    [[nodiscard]] BackendDescriptor descriptor() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> createDevices() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;
};

} // namespace lumacore
