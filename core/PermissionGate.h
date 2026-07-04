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
    // Canonical writability policy: a device counts as writable when writes are
    // granted outright or only pending per-session confirmation.
    [[nodiscard]] static bool allowsWriteOrConfirmation(const PermissionResult& permission);
    [[nodiscard]] static bool writeAllowedOrConfirmable(const RgbDevice& device);
    [[nodiscard]] static PermissionResult withSessionConfirmation(
        const PermissionResult& permission,
        bool writeConfirmed
    );
};

} // namespace lumacore
