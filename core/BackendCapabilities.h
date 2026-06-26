// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFlags>
#include <QString>

namespace lumacore {

enum class BackendCapability {
    None = 0,
    DiscoveryRead = 1 << 0,
    ZoneColorWrite = 1 << 1,
    ZoneEffectWrite = 1 << 2,
};
Q_DECLARE_FLAGS(BackendCapabilities, BackendCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(BackendCapabilities)

enum class PermissionStatus {
    Granted,
    Denied,
    RequiresConfirmation,
    NotApplicable,
};

struct PermissionResult {
    PermissionStatus status {PermissionStatus::Denied};
    QString reason;

    [[nodiscard]] bool isGranted() const { return status == PermissionStatus::Granted; }
};

[[nodiscard]] QString backendCapabilityToString(BackendCapability capability);

} // namespace lumacore
