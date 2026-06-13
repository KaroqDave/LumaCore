#pragma once

#include "core/RgbBackend.h"
#include "ipc/DaemonClient.h"

#include <memory>

namespace lumacore {

class DaemonBackend final : public RgbBackend
{
public:
    explicit DaemonBackend(std::shared_ptr<DaemonClient> client);

    [[nodiscard]] BackendDescriptor descriptor() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> createDevices() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;

private:
    std::shared_ptr<DaemonClient> m_client;
    mutable BackendDescriptor m_descriptor {
        QStringLiteral("daemon"),
        QStringLiteral("LumaCore Daemon"),
        QStringLiteral("Privileged LumaCore daemon over a local Unix socket."),
        BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite,
    };
};

} // namespace lumacore
