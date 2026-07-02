// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/BackendCapabilities.h"

namespace lumacore {

class RgbDevice;

class PermissionGate
{
public:
    [[nodiscard]] static PermissionResult checkWrite(const RgbDevice& device, BackendCapability operation);
    [[nodiscard]] static PermissionResult checkAnyWrite(const RgbDevice& device);
    [[nodiscard]] static bool writeRequiresConfirmation(const RgbDevice& device);
};

} // namespace lumacore
