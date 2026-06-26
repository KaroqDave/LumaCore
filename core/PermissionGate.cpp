// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/PermissionGate.h"

#include "core/RgbDevice.h"

namespace lumacore {

QString backendCapabilityToString(BackendCapability capability)
{
    switch (capability) {
    case BackendCapability::DiscoveryRead:
        return QStringLiteral("DiscoveryRead");
    case BackendCapability::ZoneColorWrite:
        return QStringLiteral("ZoneColorWrite");
    case BackendCapability::ZoneEffectWrite:
        return QStringLiteral("ZoneEffectWrite");
    case BackendCapability::None:
        break;
    }

    return QStringLiteral("None");
}

PermissionResult PermissionGate::checkWrite(const RgbDevice& device, BackendCapability operation)
{
    if (operation == BackendCapability::None) {
        return {PermissionStatus::NotApplicable, QStringLiteral("No operation requested.")};
    }

    const BackendCapabilities capabilities = device.capabilities();
    if (!capabilities.testFlag(operation)) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("%1 does not support %2.")
                .arg(device.name(), backendCapabilityToString(operation)),
        };
    }

    return device.checkRuntimePermission(operation);
}

} // namespace lumacore
