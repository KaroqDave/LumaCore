// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "backends/asus/AsusAuraHidPlatform.h"
#include "core/RgbBackend.h"

namespace lumacore {

class AsusAuraHidBackend final : public RgbBackend
{
public:
    [[nodiscard]] BackendDescriptor descriptor() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;

    [[nodiscard]] static bool acceptsProbeDevice(const asus_aura_platform::ProbeDevice& device);
    [[nodiscard]] static bool prefersProbeDevice(
        const asus_aura_platform::ProbeDevice& candidate,
        const asus_aura_platform::ProbeDevice& current
    );
};

} // namespace lumacore
