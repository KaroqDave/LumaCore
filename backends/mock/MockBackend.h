#pragma once

#include "core/RgbDevice.h"

#include <memory>
#include <vector>

namespace lumacore {

class MockBackend
{
public:
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> createDevices() const;
};

} // namespace lumacore
