// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/PermissionGate.h"

#include "core/RgbDevice.h"

#include <QStringList>

namespace lumacore {

namespace {

constexpr BackendCapability kWriteCapabilities[] {
    BackendCapability::ZoneColorWrite,
    BackendCapability::ZoneEffectWrite,
};

} // namespace

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

PermissionResult PermissionGate::checkAnyWrite(const RgbDevice& device)
{
    bool sawSupportedWrite = false;
    bool sawGrantedWrite = false;
    QStringList deniedReasons;

    for (const BackendCapability capability : kWriteCapabilities) {
        if (!device.capabilities().testFlag(capability)) {
            continue;
        }

        sawSupportedWrite = true;
        const PermissionResult permission = checkWrite(device, capability);
        if (permission.status == PermissionStatus::RequiresConfirmation) {
            return permission;
        }
        if (permission.isGranted()) {
            sawGrantedWrite = true;
            continue;
        }
        if (!permission.reason.isEmpty()) {
            deniedReasons.append(permission.reason);
        }
    }

    if (sawGrantedWrite) {
        return {
            PermissionStatus::Granted,
            QStringLiteral("At least one RGB write operation is available."),
        };
    }

    if (!sawSupportedWrite) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("%1 does not support RGB writes.").arg(device.name()),
        };
    }

    deniedReasons.removeDuplicates();
    return {
        PermissionStatus::Denied,
        deniedReasons.isEmpty()
            ? QStringLiteral("%1 does not currently allow RGB writes.").arg(device.name())
            : deniedReasons.join(QStringLiteral(" ")),
    };
}

bool PermissionGate::allowsWriteOrConfirmation(const PermissionResult& permission)
{
    return permission.isGranted() || permission.status == PermissionStatus::RequiresConfirmation;
}

bool PermissionGate::writeAllowedOrConfirmable(const RgbDevice& device)
{
    return allowsWriteOrConfirmation(checkAnyWrite(device));
}

bool PermissionGate::writeRequiresConfirmation(const RgbDevice& device)
{
    for (const BackendCapability capability : kWriteCapabilities) {
        if (!device.capabilities().testFlag(capability)) {
            continue;
        }
        if (checkWrite(device, capability).status == PermissionStatus::RequiresConfirmation) {
            return true;
        }
    }
    return false;
}

PermissionResult PermissionGate::withSessionConfirmation(
    const PermissionResult& permission,
    bool writeConfirmed
)
{
    if (writeConfirmed && permission.status == PermissionStatus::RequiresConfirmation) {
        return {
            PermissionStatus::Granted,
            QStringLiteral("Hardware writes are confirmed for this daemon session."),
        };
    }
    return permission;
}

} // namespace lumacore
