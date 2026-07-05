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

std::vector<std::unique_ptr<RgbDevice>> DaemonBackend::discoverDevices() const
{
    // Devices arrive exclusively through asynchronous snapshots
    // (devicesFromPayload + DeviceManager::replaceDevices); startup discovery
    // must not block on the daemon connection.
    m_lastDiscoverError.clear();
    return {};
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

BackendDescriptor DaemonBackend::effectiveDescriptor() const
{
    return m_effectiveDescriptor.id.isEmpty() ? m_descriptor : m_effectiveDescriptor;
}

QString DaemonBackend::lastDiscoverError() const
{
    return m_lastDiscoverError;
}

void DaemonBackend::updateDescriptor(const QJsonObject& payload) const
{
    const BackendDescriptor daemonDescriptor =
        backendDescriptorFromJson(payload.value(QStringLiteral("backend")).toObject());
    if (!daemonDescriptor.id.isEmpty()) {
        m_effectiveDescriptor = daemonDescriptor;
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

    // The transport is available whenever a client exists; the connection is
    // established asynchronously and its state is surfaced through the
    // client's own signals rather than a blocking status call here.
    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
