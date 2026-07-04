// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/windows/ProbeResult.h"

#include "hardware/common/ProbeResultCommon.h"

namespace lumacore::hardware::windows {

QString stableProbeId(const QString& source, const QString& key)
{
    return common::stableProbeId(source, key, QStringLiteral("windows-discovery-device"));
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

} // namespace lumacore::hardware::windows
