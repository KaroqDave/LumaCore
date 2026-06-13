#pragma once

#include "hardware/linux/ProbeResult.h"

namespace lumacore::hardware::linux {

[[nodiscard]] bool hidapiProbeAvailable();
[[nodiscard]] ProbeResult probeHidapiDevices();

} // namespace lumacore::hardware::linux
