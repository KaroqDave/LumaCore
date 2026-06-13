#pragma once

#include "core/BackendCapabilities.h"

#include <QString>

#include <memory>
#include <vector>

namespace lumacore {

class RgbDevice;

struct BackendDescriptor {
    QString id;
    QString displayName;
    QString description;
    BackendCapabilities capabilities;
};

class RgbBackend
{
public:
    virtual ~RgbBackend() = default;

    [[nodiscard]] virtual BackendDescriptor descriptor() const = 0;
    [[nodiscard]] virtual std::vector<std::unique_ptr<RgbDevice>> createDevices() const = 0;
    [[nodiscard]] virtual std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const;
    [[nodiscard]] virtual PermissionResult probe() const;
};

[[nodiscard]] QString backendCapabilityDisplayName(BackendCapability capability);

} // namespace lumacore
