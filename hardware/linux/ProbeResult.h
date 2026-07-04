// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "hardware/common/ProbeTypes.h"

namespace lumacore::hardware::linux {

using ProbeDevice = common::ProbeDevice;
using ProbeResult = common::ProbeResult;
using DiscoverySupportInfo = common::DiscoverySupportInfo;

[[nodiscard]] QString stableProbeId(const QString& source, const QString& key);
[[nodiscard]] QString hexWord(quint16 value);
[[nodiscard]] QString usbVidPidKey(const ProbeDevice& device);
[[nodiscard]] DiscoverySupportInfo discoverySupportInfo(const ProbeDevice& device);
[[nodiscard]] bool isCatalogedRgbController(const ProbeDevice& device);
[[nodiscard]] bool isLikelyRgbController(const ProbeDevice& device);

} // namespace lumacore::hardware::linux
