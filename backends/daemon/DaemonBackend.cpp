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
    std::vector<std::unique_ptr<RgbDevice>> devices;
    if (m_client == nullptr) {
        return devices;
    }

    const DaemonCallResult response = m_client->call(QStringLiteral("listDevices"));
    if (!response.ok) {
        return devices;
    }

    const BackendDescriptor daemonDescriptor = backendDescriptorFromJson(response.result.value(QStringLiteral("backend")).toObject());
    if (!daemonDescriptor.id.isEmpty()) {
        m_descriptor.displayName = QStringLiteral("Daemon: %1").arg(daemonDescriptor.displayName);
        m_descriptor.description = QStringLiteral("%1 Socket: %2")
                                       .arg(daemonDescriptor.description, m_client->socketPath());
        m_descriptor.capabilities = daemonDescriptor.capabilities;
    }

    const QJsonArray snapshots = response.result.value(QStringLiteral("devices")).toArray();
    devices.reserve(static_cast<std::size_t>(snapshots.size()));
    for (const QJsonValue& snapshot : snapshots) {
        devices.push_back(std::make_unique<DaemonRgbDevice>(snapshot.toObject(), m_client));
    }

    return devices;
}

PermissionResult DaemonBackend::probe() const
{
    if (m_client == nullptr) {
        return {PermissionStatus::Denied, QStringLiteral("Daemon client is not available.")};
    }

    const DaemonCallResult response = m_client->call(QStringLiteral("status"));
    if (!response.ok) {
        return {PermissionStatus::Denied, response.error};
    }

    const BackendDescriptor daemonDescriptor = backendDescriptorFromJson(response.result.value(QStringLiteral("backend")).toObject());
    if (!daemonDescriptor.id.isEmpty()) {
        m_descriptor.displayName = QStringLiteral("Daemon: %1").arg(daemonDescriptor.displayName);
        m_descriptor.description = QStringLiteral("%1 Socket: %2")
                                       .arg(daemonDescriptor.description, m_client->socketPath());
        m_descriptor.capabilities = daemonDescriptor.capabilities;
    }

    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
