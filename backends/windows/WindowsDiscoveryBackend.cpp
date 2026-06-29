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

std::vector<std::unique_ptr<RgbDevice>> WindowsDiscoveryBackend::createDevices() const
{
    return discoverDevices();
}

std::vector<std::unique_ptr<RgbDevice>> WindowsDiscoveryBackend::discoverDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    QSet<QString> seenIds;
    QSet<QString> seenControllers;

    for (const hardware::windows::ProbeResult& result : collectProbeResults()) {
        if (!result.providerAvailable) {
            continue;
        }

        for (const hardware::windows::ProbeDevice& probeDevice : result.devices) {
            if (probeDevice.id.isEmpty() || seenIds.contains(probeDevice.id)) {
                continue;
            }

            // A single physical peripheral exposes one hid_device_info entry per
            // top-level HID collection, all sharing one VID:PID but with distinct
            // paths. Collapse them into a single read-only inventory row so a
            // keyboard/mouse/controller does not appear several times.
            const QString controllerKey = hardware::windows::usbVidPidKey(probeDevice);
            if (!controllerKey.isEmpty()) {
                if (seenControllers.contains(controllerKey)) {
                    continue;
                }
                seenControllers.insert(controllerKey);
            }

            seenIds.insert(probeDevice.id);
            devices.push_back(std::make_unique<WindowsDiscoveredDevice>(probeDevice));
        }
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
