#pragma once

#include "hardware/linux/ProbeResult.h"

namespace lumacore::hardware::linux {

[[nodiscard]] bool i2cDevProbeAvailable();
[[nodiscard]] ProbeResult probeI2cDevAdapters();

} // namespace lumacore::hardware::linux
