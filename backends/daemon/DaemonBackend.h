// SPDX-License-Identifier: GPL-2.0-or-later

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
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;
    [[nodiscard]] QString lastDiscoverError() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> devicesFromPayload(const QJsonObject& payload) const;
    [[nodiscard]] BackendDescriptor effectiveDescriptor() const;

private:
    void updateDescriptor(const QJsonObject& payload) const;

    std::shared_ptr<DaemonClient> m_client;
    mutable BackendDescriptor m_descriptor {
        QStringLiteral("daemon"),
        QStringLiteral("LumaCore Daemon"),
        QStringLiteral("LumaCore daemon over a local IPC endpoint."),
        BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite,
    };
    mutable BackendDescriptor m_effectiveDescriptor;
    mutable QString m_lastDiscoverError;
};

} // namespace lumacore
