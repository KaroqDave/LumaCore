// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/daemon/DaemonBackend.h"

#include "backends/daemon/DaemonRgbDevice.h"
#include "ipc/DaemonProtocol.h"

#include <QJsonArray>

#include <utility>

namespace lumacore {

DaemonBackend::DaemonBackend(std::shared_ptr<DaemonClient> client)
    : m_client(std::move(client))
{
}

BackendDescriptor DaemonBackend::descriptor() const
{
    return m_descriptor;
}

std::vector<std::unique_ptr<RgbDevice>> DaemonBackend::createDevices() const
{
    return discoverDevices();
}

std::vector<std::unique_ptr<RgbDevice>> DaemonBackend::discoverDevices() const
{
    if (m_client == nullptr) {
        return {};
    }

    const DaemonCallResult response = m_client->call(daemonMethodName(DaemonMethod::ListDevices));
    if (!response.ok) {
        return {};
    }

    return devicesFromPayload(response.result);
}

std::vector<std::unique_ptr<RgbDevice>> DaemonBackend::devicesFromPayload(const QJsonObject& payload) const
{
    updateDescriptor(payload);
    std::vector<std::unique_ptr<RgbDevice>> devices;
    const QJsonArray snapshots = payload.value(QStringLiteral("devices")).toArray();
    devices.reserve(static_cast<std::size_t>(snapshots.size()));
    for (const QJsonValue& snapshot : snapshots) {
        devices.push_back(std::make_unique<DaemonRgbDevice>(snapshot.toObject(), m_client));
    }
    return devices;
}

void DaemonBackend::updateDescriptor(const QJsonObject& payload) const
{
    const BackendDescriptor daemonDescriptor =
        backendDescriptorFromJson(payload.value(QStringLiteral("backend")).toObject());
    if (!daemonDescriptor.id.isEmpty()) {
        m_descriptor.displayName = QStringLiteral("Daemon: %1").arg(daemonDescriptor.displayName);
        m_descriptor.description = QStringLiteral("%1 Socket: %2")
                                       .arg(daemonDescriptor.description, m_client->socketPath());
        m_descriptor.capabilities = daemonDescriptor.capabilities;
    }
}

PermissionResult DaemonBackend::probe() const
{
    if (m_client == nullptr) {
        return {PermissionStatus::Denied, QStringLiteral("Daemon client is not available.")};
    }

    const DaemonCallResult response = m_client->call(daemonMethodName(DaemonMethod::Status));
    if (!response.ok) {
        return {PermissionStatus::Denied, response.error};
    }

    updateDescriptor(response.result);

    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
