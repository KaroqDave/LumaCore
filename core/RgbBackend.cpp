// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/RgbBackend.h"

#include "core/RgbDevice.h"

namespace lumacore {

std::vector<std::unique_ptr<RgbDevice>> RgbBackend::discoverDevices() const
{
    return createDevices();
}

PermissionResult RgbBackend::probe() const
{
    return {PermissionStatus::Granted, {}};
}

QString backendCapabilityDisplayName(BackendCapability capability)
{
    switch (capability) {
    case BackendCapability::DiscoveryRead:
        return QStringLiteral("Discovery");
    case BackendCapability::ZoneColorWrite:
        return QStringLiteral("Color write");
    case BackendCapability::ZoneEffectWrite:
        return QStringLiteral("Effect write");
    case BackendCapability::None:
        break;
    }

    return QStringLiteral("None");
}

} // namespace lumacore
