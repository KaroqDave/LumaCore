// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "hardware/windows/ProbeResult.h"

namespace lumacore::hardware::windows {

[[nodiscard]] bool hidapiProbeAvailable();
[[nodiscard]] ProbeResult probeHidapiDevices();

} // namespace lumacore::hardware::windows
