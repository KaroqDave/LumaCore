#pragma once

#include "core/RgbDevice.h"

namespace lumacore {

class MockRgbDevice final : public RgbDevice
{
    Q_OBJECT

public:
    explicit MockRgbDevice(QObject* parent = nullptr);

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override;
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability capability) const override;
};

} // namespace lumacore
