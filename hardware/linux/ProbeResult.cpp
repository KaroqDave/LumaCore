// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/linux/ProbeResult.h"

#include "hardware/common/ProbeResultCommon.h"

namespace lumacore::hardware::linux {

QString stableProbeId(const QString& source, const QString& key)
{
    return common::stableProbeId(source, key, QStringLiteral("linux-discovery-device"));
}

QString hexWord(quint16 value)
{
    return common::hexWord(value);
}

QString usbVidPidKey(const ProbeDevice& device)
{
    return common::usbVidPidKey(device);
}

DiscoverySupportInfo discoverySupportInfo(const ProbeDevice& device)
{
    return common::discoverySupportInfo(
        device,
        QStringLiteral("No RGB controller catalog entry or heuristic match.")
    );
}

bool isCatalogedRgbController(const ProbeDevice& device)
{
    return common::isCatalogedRgbController(device);
}

bool isLikelyRgbController(const ProbeDevice& device)
{
    return common::isLikelyRgbController(device);
}

} // namespace lumacore::hardware::linux
