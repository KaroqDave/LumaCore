// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/auto/AutoBackend.h"

#ifdef LUMACORE_HAS_ASUS_AURA_HID
#include "backends/asus/AsusAuraHidBackend.h"
#endif
#ifdef LUMACORE_HAS_LINUX_DISCOVERY
#include "backends/linux/LinuxDiscoveryBackend.h"
#endif
#ifdef LUMACORE_HAS_WINDOWS_DISCOVERY
#include "backends/windows/WindowsDiscoveryBackend.h"
#endif
#include "backends/mock/MockBackend.h"
#include "core/RgbDevice.h"

#include <memory>
#include <utility>

namespace lumacore {

AutoBackend::AutoBackend(QString mockScenarioId)
    : m_mockScenarioId(std::move(mockScenarioId))
{
}

QString AutoBackend::backendId()
{
    return QStringLiteral("auto");
}

bool AutoBackend::isRepresentedDiscoveryDevice(
    const RgbDevice& device,
    const QSet<QString>& representedIdentities
)
{
    const QString identity = device.discoveryIdentity();
    return !identity.isEmpty() && representedIdentities.contains(identity);
}

BackendDescriptor AutoBackend::descriptor() const
{
    return {
        backendId(),
        QStringLiteral("Auto Backend"),
        QStringLiteral("Aggregates available hardware backends and falls back to mock devices."),
        // Aggregated hardware backends can write, so the descriptor advertises the
        // write capabilities its devices expose; per-device permission/write gates
        // still decide whether any individual write is allowed.
        BackendCapability::DiscoveryRead
            | BackendCapability::ZoneColorWrite
            | BackendCapability::ZoneEffectWrite,
    };
}

std::vector<std::unique_ptr<RgbDevice>> AutoBackend::discoverDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    QSet<QString> representedIdentities;

#ifdef LUMACORE_HAS_ASUS_AURA_HID
    AsusAuraHidBackend asusBackend;
    if (asusBackend.probe().isGranted()) {
        for (std::unique_ptr<RgbDevice>& device : asusBackend.discoverDevices()) {
            if (device) {
                const QString identity = device->discoveryIdentity();
                if (!identity.isEmpty()) {
                    representedIdentities.insert(identity);
                }
                device->setBackendId(QStringLiteral("asus-aura-hid"));
                devices.push_back(std::move(device));
            }
        }
    }
#endif

#ifdef LUMACORE_HAS_LINUX_DISCOVERY
    LinuxDiscoveryBackend linuxDiscoveryBackend;
    if (linuxDiscoveryBackend.probe().isGranted()) {
        for (std::unique_ptr<RgbDevice>& device : linuxDiscoveryBackend.discoverDevices()) {
            if (!device || isRepresentedDiscoveryDevice(*device, representedIdentities)) {
                continue;
            }
            device->setBackendId(QStringLiteral("linux-discovery"));
            devices.push_back(std::move(device));
        }
    }
#endif

#ifdef LUMACORE_HAS_WINDOWS_DISCOVERY
    WindowsDiscoveryBackend windowsDiscoveryBackend;
    if (windowsDiscoveryBackend.probe().isGranted()) {
        for (std::unique_ptr<RgbDevice>& device : windowsDiscoveryBackend.discoverDevices()) {
            if (!device || isRepresentedDiscoveryDevice(*device, representedIdentities)) {
                continue;
            }
            device->setBackendId(QStringLiteral("windows-discovery"));
            devices.push_back(std::move(device));
        }
    }
#endif

    if (devices.empty()) {
        MockBackend mockBackend(m_mockScenarioId);
        for (std::unique_ptr<RgbDevice>& device : mockBackend.discoverDevices()) {
            if (device) {
                device->setBackendId(QStringLiteral("mock"));
                devices.push_back(std::move(device));
            }
        }
    }

    return devices;
}

PermissionResult AutoBackend::probe() const
{
    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
