// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "hardware/common/ProbeTypes.h"

namespace lumacore::hardware::common {

[[nodiscard]] QString stableProbeId(const QString& source, const QString& key, const QString& fallbackId);
[[nodiscard]] QString hexWord(quint16 value);
[[nodiscard]] QString usbVidPidKey(const ProbeDevice& device);
[[nodiscard]] DiscoverySupportInfo discoverySupportInfo(
    const ProbeDevice& device,
    const QString& unclassifiedSummary
);
[[nodiscard]] bool isCatalogedRgbController(const ProbeDevice& device);
[[nodiscard]] bool isLikelyRgbController(const ProbeDevice& device);

} // namespace lumacore::hardware::common
