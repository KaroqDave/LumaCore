#include "backends/linux/LinuxDiscoveredDevice.h"

namespace lumacore {

LinuxDiscoveredDevice::LinuxDiscoveredDevice(const hardware::linux::ProbeDevice& device, QObject* parent)
    : RgbDevice(device.id, device.name, device.vendor, device.type, parent)
    , m_discoveryIdentity(hardware::linux::usbVidPidKey(device))
    , m_source(device.source)
    , m_path(device.path)
    , m_details(device.details)
{
    setLikelyRgbController(hardware::linux::isLikelyRgbController(device));
    mutableZones().append(RgbZone(QStringLiteral("Read-only discovery"), RgbZoneType::Unknown, 1, RgbColor(96, 96, 96)));
    mutableZones()[0].setEffect(RgbEffect(RgbEffectType::Static, RgbColor(96, 96, 96)));
}

QString LinuxDiscoveredDevice::discoveryIdentity() const
{
    return m_discoveryIdentity;
}

const QString& LinuxDiscoveredDevice::source() const
{
    return m_source;
}

const QString& LinuxDiscoveredDevice::path() const
{
    return m_path;
}

const QString& LinuxDiscoveredDevice::details() const
{
    return m_details;
}

bool LinuxDiscoveredDevice::setZoneStaticColor(int zoneIndex, const RgbColor& color)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(color)
    return false;
}

bool LinuxDiscoveredDevice::applyZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(effect)
    return false;
}

bool LinuxDiscoveredDevice::applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(colors)
    return false;
}

bool LinuxDiscoveredDevice::updateZoneMetadata(int zoneIndex, const QString& name, int ledCount)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(name)
    Q_UNUSED(ledCount)
    return false;
}

bool LinuxDiscoveredDevice::usesLocalFrameRendering() const
{
    return false;
}

BackendCapabilities LinuxDiscoveredDevice::capabilities() const
{
    return BackendCapability::DiscoveryRead;
}

PermissionResult LinuxDiscoveredDevice::checkRuntimePermission(BackendCapability capability) const
{
    if (capability == BackendCapability::DiscoveryRead) {
        return {PermissionStatus::Granted, {}};
    }

    return {
        PermissionStatus::Denied,
        QStringLiteral("%1 is a read-only discovery candidate from %2. Writes are disabled in this phase.")
            .arg(name(), m_source),
    };
}

} // namespace lumacore
