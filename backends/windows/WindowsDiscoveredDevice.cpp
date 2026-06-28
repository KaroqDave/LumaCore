// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/windows/WindowsDiscoveredDevice.h"

namespace lumacore {

WindowsDiscoveredDevice::WindowsDiscoveredDevice(const hardware::windows::ProbeDevice& device, QObject* parent)
    : RgbDevice(device.id, device.name, device.vendor, device.type, parent)
    , m_discoveryIdentity(hardware::windows::usbVidPidKey(device))
    , m_source(device.source)
    , m_path(device.path)
    , m_details(device.details)
    , m_support(hardware::windows::discoverySupportInfo(device))
{
    setLikelyRgbController(m_support.likelyRgbController);
    mutableZones().append(RgbZone(QStringLiteral("Read-only discovery"), RgbZoneType::Unknown, 1, RgbColor(96, 96, 96)));
    mutableZones()[0].setEffect(RgbEffect(RgbEffectType::Static, RgbColor(96, 96, 96)));
}

QString WindowsDiscoveredDevice::discoveryIdentity() const
{
    return m_discoveryIdentity;
}

QString WindowsDiscoveredDevice::discoverySupportStage() const
{
    return m_support.stage;
}

QString WindowsDiscoveredDevice::discoverySupportStatus() const
{
    return m_support.status;
}

QString WindowsDiscoveredDevice::discoverySupportFamily() const
{
    return m_support.family;
}

QString WindowsDiscoveredDevice::discoverySupportNotes() const
{
    return m_support.notes;
}

bool WindowsDiscoveredDevice::discoveryCataloged() const
{
    return m_support.cataloged;
}

bool WindowsDiscoveredDevice::discoveryWriteCapableBackend() const
{
    return m_support.writeCapableBackend;
}

const QString& WindowsDiscoveredDevice::source() const
{
    return m_source;
}

const QString& WindowsDiscoveredDevice::path() const
{
    return m_path;
}

const QString& WindowsDiscoveredDevice::details() const
{
    return m_details;
}

bool WindowsDiscoveredDevice::setZoneStaticColor(int zoneIndex, const RgbColor& color)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(color)
    return false;
}

bool WindowsDiscoveredDevice::applyZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(effect)
    return false;
}

bool WindowsDiscoveredDevice::applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(colors)
    return false;
}

bool WindowsDiscoveredDevice::updateZoneMetadata(int zoneIndex, const QString& name, int ledCount)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(name)
    Q_UNUSED(ledCount)
    return false;
}

bool WindowsDiscoveredDevice::usesLocalFrameRendering() const
{
    return false;
}

BackendCapabilities WindowsDiscoveredDevice::capabilities() const
{
    return BackendCapability::DiscoveryRead;
}

PermissionResult WindowsDiscoveredDevice::checkRuntimePermission(BackendCapability capability) const
{
    if (capability == BackendCapability::DiscoveryRead) {
        return {PermissionStatus::Granted, {}};
    }

    return {
        PermissionStatus::Denied,
        QStringLiteral("%1 is a read-only Windows HID discovery candidate from %2. %3")
            .arg(name(), m_source, m_support.summary),
    };
}

} // namespace lumacore
