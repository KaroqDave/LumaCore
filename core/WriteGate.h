#pragma once

#include "core/BackendCapabilities.h"

namespace lumacore {

class ActivityLog;
class RgbDevice;

class WriteGate
{
public:
    struct Outcome {
        bool applyWrite {false};
        bool reportSuccess {false};
    };

    [[nodiscard]] static Outcome evaluate(
        bool dryRunEnabled,
        const RgbDevice& device,
        BackendCapability operation,
        ActivityLog& log,
        const QString& dryRunSummary,
        bool confirmationGranted = false
    );
};

} // namespace lumacore
