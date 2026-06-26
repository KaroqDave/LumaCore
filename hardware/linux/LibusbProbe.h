// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "hardware/linux/ProbeResult.h"

namespace lumacore::hardware::linux {

[[nodiscard]] bool libusbProbeAvailable();
[[nodiscard]] ProbeResult probeLibusbDevices();

} // namespace lumacore::hardware::linux
