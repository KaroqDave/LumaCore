// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbBackend.h"
#include "hardware/windows/ProbeResult.h"

#include <QVector>

namespace lumacore {

[[nodiscard]] QVector<hardware::windows::ProbeDevice> windowsDiscoveryInventoryDevices(
    const QVector<hardware::windows::ProbeResult>& results
);

class WindowsDiscoveryBackend final : public RgbBackend
{
public:
    [[nodiscard]] BackendDescriptor descriptor() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> createDevices() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;
};

} // namespace lumacore
