#include "core/WriteGate.h"

#include "core/ActivityLog.h"
#include "core/PermissionGate.h"
#include "core/RgbDevice.h"

namespace lumacore {

WriteGate::Outcome WriteGate::evaluate(
    bool dryRunEnabled,
    const RgbDevice& device,
    BackendCapability operation,
    ActivityLog& log,
    const QString& dryRunSummary,
    bool confirmationGranted
)
{
    const PermissionResult permission = PermissionGate::checkWrite(device, operation);
    if (permission.status == PermissionStatus::RequiresConfirmation) {
        if (confirmationGranted) {
            if (dryRunEnabled) {
                log.warning(
                    LogCategory::Permission,
                    permission.reason.isEmpty()
                        ? QStringLiteral("%1 requires confirmation before %2.").arg(device.name(), backendCapabilityToString(operation))
                        : permission.reason
                );
                log.info(LogCategory::Backend, QStringLiteral("[DRY-RUN] %1").arg(dryRunSummary));
                return {false, true};
            }

            return {true, true};
        }

        if (dryRunEnabled) {
            log.warning(
                LogCategory::Permission,
                permission.reason.isEmpty()
                    ? QStringLiteral("%1 requires confirmation before %2.").arg(device.name(), backendCapabilityToString(operation))
                    : permission.reason
            );
            log.info(LogCategory::Backend, QStringLiteral("[DRY-RUN] %1").arg(dryRunSummary));
            return {false, true};
        }

        log.warning(
            LogCategory::Permission,
            permission.reason.isEmpty()
                ? QStringLiteral("%1 requires confirmation before %2.").arg(device.name(), backendCapabilityToString(operation))
                : permission.reason
        );
        return {};
    }

    if (!permission.isGranted()) {
        log.warning(LogCategory::Permission, permission.reason);
        return {};
    }

    if (dryRunEnabled) {
        log.info(LogCategory::Backend, QStringLiteral("[DRY-RUN] %1").arg(dryRunSummary));
        return {false, true};
    }

    return {true, true};
}

} // namespace lumacore
