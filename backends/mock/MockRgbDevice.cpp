// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockRgbDevice.h"

namespace lumacore {

namespace {

constexpr BackendCapabilities kMockCapabilities =
    BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite;

MockRgbDeviceConfig defaultConfig()
{
    MockRgbDeviceConfig config;
    config.id = QStringLiteral("mock-asus-tuf-x870-plus-wifi");
    config.name = QStringLiteral("Mock ASUS TUF X870-PLUS WIFI");
    config.vendor = QStringLiteral("ASUS");
    config.type = RgbDeviceType::Motherboard;
    config.capabilities = kMockCapabilities;
    config.runtimeWritePermission = {PermissionStatus::Granted, {}};
    config.likelyRgbController = true;

    // Modern blue-ish defaults so previews and zone swatches are inviting out of the box.
    const RgbColor header1Color(30, 84, 214);   // #1E54D6
    const RgbColor header2Color(34, 211, 238);  // #22D3EE
    const RgbColor header3Color(124, 92, 255);  // #7C5CFF

    config.zones.append(RgbZone(QStringLiteral("Header 1"), RgbZoneType::Motherboard, 10, header1Color));
    config.zones.append(RgbZone(QStringLiteral("Header 2"), RgbZoneType::AddressableHeader, 30, header2Color));
    config.zones.append(RgbZone(QStringLiteral("Header 3"), RgbZoneType::AddressableHeader, 30, header3Color));
    return config;
}

} // namespace

MockRgbDevice::MockRgbDevice(QObject* parent)
    : MockRgbDevice(defaultConfig(), parent)
{
}

MockRgbDevice::MockRgbDevice(MockRgbDeviceConfig config, QObject* parent)
    : RgbDevice(config.id, config.name, config.vendor, config.type, parent)
    , m_capabilities(config.capabilities)
    , m_runtimeWritePermission(config.runtimeWritePermission)
    , m_failWrites(config.failWrites)
{
    setLikelyRgbController(config.likelyRgbController);

    for (const RgbZone& zone : config.zones) {
        mutableZones().append(zone);
        mutableZones().last().setEffect(RgbEffect(RgbEffectType::Static, zone.currentColor()));
    }
}

bool MockRgbDevice::setZoneStaticColor(int zoneIndex, const RgbColor& color)
{
    if (m_failWrites) {
        return rejectWrite(QStringLiteral("static color"));
    }

    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }

    mutableZones()[zoneIndex].setColor(color);
    emit zoneChanged(zoneIndex);
    return true;
}

bool MockRgbDevice::applyZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    if (m_failWrites) {
        return rejectWrite(QStringLiteral("effect"));
    }

    return RgbDevice::applyZoneEffect(zoneIndex, effect);
}

bool MockRgbDevice::applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors)
{
    if (m_failWrites) {
        return rejectWrite(QStringLiteral("frame"));
    }

    return RgbDevice::applyZoneFrame(zoneIndex, colors);
}

bool MockRgbDevice::applyAllOff()
{
    if (m_failWrites) {
        return rejectWrite(QStringLiteral("all-off"));
    }

    const RgbEffect offEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0);
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        setZoneEffect(zoneIndex, offEffect);
        mutableZones()[zoneIndex].setColor(RgbColor(0, 0, 0));
        emit zoneChanged(zoneIndex);
    }
    return true;
}

QString MockRgbDevice::lastHardwareWriteStatus() const
{
    return m_lastHardwareWriteStatus;
}

BackendCapabilities MockRgbDevice::capabilities() const
{
    return m_capabilities;
}

PermissionResult MockRgbDevice::checkRuntimePermission(BackendCapability capability) const
{
    if (!m_capabilities.testFlag(capability)) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("Mock backend does not support %1.").arg(backendCapabilityToString(capability)),
        };
    }

    if (capability == BackendCapability::DiscoveryRead) {
        return {PermissionStatus::Granted, {}};
    }

    return m_runtimeWritePermission;
}

bool MockRgbDevice::rejectWrite(const QString& operation)
{
    m_lastHardwareWriteStatus = QStringLiteral("Mock failing-writes scenario rejected %1 write.").arg(operation);
    return false;
}

} // namespace lumacore
