// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "hardware/linux/ProbeResult.h"

namespace lumacore::hardware::linux {

[[nodiscard]] bool hidapiProbeAvailable();
[[nodiscard]] ProbeResult probeHidapiDevices();

} // namespace lumacore::hardware::linux
