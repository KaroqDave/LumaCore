#include "backends/linux/LinuxDiscoveryBackend.h"

#include "backends/linux/LinuxDiscoveredDevice.h"
#include "hardware/linux/HidapiProbe.h"
#include "hardware/linux/I2cDevProbe.h"
#include "hardware/linux/LibusbProbe.h"

#include <QSet>
#include <QStringList>

namespace lumacore {

namespace {

QVector<hardware::linux::ProbeResult> collectProbeResults()
{
    return {
        hardware::linux::probeHidapiDevices(),
        hardware::linux::probeLibusbDevices(),
        hardware::linux::probeI2cDevAdapters(),
    };
}

QString providerSummary()
{
    QStringList providers;
    if (hardware::linux::hidapiProbeAvailable()) {
        providers.append(QStringLiteral("hidapi"));
    }
    if (hardware::linux::libusbProbeAvailable()) {
        providers.append(QStringLiteral("libusb"));
    }
    if (hardware::linux::i2cDevProbeAvailable()) {
        providers.append(QStringLiteral("i2c-dev"));
    }
    return providers.isEmpty() ? QStringLiteral("no providers compiled in") : providers.join(QStringLiteral(", "));
}

QString deviceVidPidKey(const hardware::linux::ProbeDevice& device)
{
    if (device.vendorId.isEmpty() || device.productId.isEmpty()) {
        return {};
    }
    return QStringLiteral("%1:%2").arg(device.vendorId.toUpper(), device.productId.toUpper());
}

} // namespace

BackendDescriptor LinuxDiscoveryBackend::descriptor() const
{
    return {
        QStringLiteral("linux-discovery"),
        QStringLiteral("Linux Hardware Discovery"),
        QStringLiteral("Read-only Linux hardware discovery via %1. No RGB writes are performed.").arg(providerSummary()),
        BackendCapability::DiscoveryRead,
    };
}

std::vector<std::unique_ptr<RgbDevice>> LinuxDiscoveryBackend::createDevices() const
{
    return discoverDevices();
}

std::vector<std::unique_ptr<RgbDevice>> LinuxDiscoveryBackend::discoverDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    QSet<QString> seenIds;
    QSet<QString> seenHidVidPid;

    for (const hardware::linux::ProbeResult& result : collectProbeResults()) {
        if (!result.providerAvailable) {
            continue;
        }

        for (const hardware::linux::ProbeDevice& probeDevice : result.devices) {
            const QString vidPid = deviceVidPidKey(probeDevice);
            if (probeDevice.source == QStringLiteral("hidapi") && !vidPid.isEmpty()) {
                seenHidVidPid.insert(vidPid);
            } else if (probeDevice.source == QStringLiteral("libusb") && !vidPid.isEmpty() && seenHidVidPid.contains(vidPid)) {
                continue;
            }

            if (probeDevice.id.isEmpty() || seenIds.contains(probeDevice.id)) {
                continue;
            }

            seenIds.insert(probeDevice.id);
            devices.push_back(std::make_unique<LinuxDiscoveredDevice>(probeDevice));
        }
    }

    return devices;
}

PermissionResult LinuxDiscoveryBackend::probe() const
{
    if (!hardware::linux::hidapiProbeAvailable()
        && !hardware::linux::libusbProbeAvailable()
        && !hardware::linux::i2cDevProbeAvailable()) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("Linux discovery backend has no compiled providers. Enable hidapi, libusb, or i2c-dev detection."),
        };
    }

    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
