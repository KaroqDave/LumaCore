#pragma once

#include "core/BackendCapabilities.h"

namespace lumacore {

class RgbDevice;

class PermissionGate
{
public:
    [[nodiscard]] static PermissionResult checkWrite(const RgbDevice& device, BackendCapability operation);
};

} // namespace lumacore
