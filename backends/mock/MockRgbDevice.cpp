// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockRgbDevice.h"

namespace lumacore {

namespace {

constexpr BackendCapabilities kMockCapabilities =
    BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite;

} // namespace

MockRgbDevice::MockRgbDevice(QObject* parent)
    : RgbDevice(
          QStringLiteral("mock-asus-tuf-x870-plus-wifi"),
          QStringLiteral("Mock ASUS TUF X870-PLUS WIFI"),
          QStringLiteral("ASUS"),
          RgbDeviceType::Motherboard,
          parent
      )
{
    setLikelyRgbController(true);

    // Modern blue-ish defaults so previews and zone swatches are inviting out of the box.
    const RgbColor header1Color(30, 84, 214);   // #1E54D6
    const RgbColor header2Color(34, 211, 238);   // #22D3EE
    const RgbColor header3Color(124, 92, 255);   // #7C5CFF

    mutableZones().append(RgbZone(QStringLiteral("Header 1"), RgbZoneType::Motherboard, 10, header1Color));
    mutableZones().append(RgbZone(QStringLiteral("Header 2"), RgbZoneType::AddressableHeader, 30, header2Color));
    mutableZones().append(RgbZone(QStringLiteral("Header 3"), RgbZoneType::AddressableHeader, 30, header3Color));

    mutableZones()[0].setEffect(RgbEffect(RgbEffectType::Static, header1Color));
    mutableZones()[1].setEffect(RgbEffect(RgbEffectType::Static, header2Color));
    mutableZones()[2].setEffect(RgbEffect(RgbEffectType::Static, header3Color));
}

bool MockRgbDevice::setZoneStaticColor(int zoneIndex, const RgbColor& color)
{
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }

    mutableZones()[zoneIndex].setColor(color);
    emit zoneChanged(zoneIndex);
    return true;
}

bool MockRgbDevice::applyAllOff()
{
    const RgbEffect offEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0);
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        setZoneEffect(zoneIndex, offEffect);
        mutableZones()[zoneIndex].setColor(RgbColor(0, 0, 0));
        emit zoneChanged(zoneIndex);
    }
    return true;
}

BackendCapabilities MockRgbDevice::capabilities() const
{
    return kMockCapabilities;
}

PermissionResult MockRgbDevice::checkRuntimePermission(BackendCapability capability) const
{
    if (kMockCapabilities.testFlag(capability)) {
        return {PermissionStatus::Granted, {}};
    }

    return {
        PermissionStatus::Denied,
        QStringLiteral("Mock backend does not support %1.").arg(backendCapabilityToString(capability)),
    };
}

} // namespace lumacore
