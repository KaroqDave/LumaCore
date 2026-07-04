// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/windows/WindowsDiscoveryBackend.h"

#include "backends/windows/WindowsDiscoveredDevice.h"
#include "hardware/windows/HidapiProbe.h"

#include <QSet>
#include <QStringList>

namespace lumacore {

namespace {

QVector<hardware::windows::ProbeResult> collectProbeResults()
{
    return {
        hardware::windows::probeHidapiDevices(),
    };
}

QString providerSummary()
{
    QStringList providers;
    if (hardware::windows::hidapiProbeAvailable()) {
        providers.append(QStringLiteral("hidapi"));
    }
    return providers.isEmpty() ? QStringLiteral("no providers compiled in") : providers.join(QStringLiteral(", "));
}

} // namespace

BackendDescriptor WindowsDiscoveryBackend::descriptor() const
{
    return {
        QStringLiteral("windows-discovery"),
        QStringLiteral("Windows HID Discovery"),
        QStringLiteral("Read-only Windows HID discovery via %1. No RGB writes are performed.").arg(providerSummary()),
        BackendCapability::DiscoveryRead,
    };
}

QVector<hardware::windows::ProbeDevice> windowsDiscoveryInventoryDevices(
    const QVector<hardware::windows::ProbeResult>& results
)
{
    QVector<hardware::windows::ProbeDevice> devices;
    QSet<QString> seenIds;

    for (const hardware::windows::ProbeResult& result : results) {
        if (!result.providerAvailable) {
            continue;
        }

        for (const hardware::windows::ProbeDevice& probeDevice : result.devices) {
            if (probeDevice.id.isEmpty() || seenIds.contains(probeDevice.id)) {
                continue;
            }

            seenIds.insert(probeDevice.id);
            devices.append(probeDevice);
        }
    }

    return devices;
}

std::vector<std::unique_ptr<RgbDevice>> WindowsDiscoveryBackend::discoverDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    const QVector<hardware::windows::ProbeDevice> probeDevices =
        windowsDiscoveryInventoryDevices(collectProbeResults());
    devices.reserve(static_cast<std::size_t>(probeDevices.size()));
    for (const hardware::windows::ProbeDevice& probeDevice : probeDevices) {
        devices.push_back(std::make_unique<WindowsDiscoveredDevice>(probeDevice));
    }

    return devices;
}

PermissionResult WindowsDiscoveryBackend::probe() const
{
    if (!hardware::windows::hidapiProbeAvailable()) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("Windows discovery backend has no compiled providers. Enable hidapi detection."),
        };
    }

    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
