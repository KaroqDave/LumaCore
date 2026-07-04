// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbBackend.h"
#include "core/RgbDevice.h"

#include <memory>
#include <vector>

namespace lumacore {

class MockBackend final : public RgbBackend
{
public:
    [[nodiscard]] BackendDescriptor descriptor() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;
};

} // namespace lumacore
