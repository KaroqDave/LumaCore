#include "backends/mock/MockRgbDevice.h"

namespace lumacore {

MockRgbDevice::MockRgbDevice(QObject* parent)
    : RgbDevice(QStringLiteral("mock-asus-tuf-x870-plus-wifi"), QStringLiteral("Mock ASUS TUF X870-PLUS WIFI"), parent)
{
    mutableZones().append(RgbZone(QStringLiteral("Motherboard"), 8));
    mutableZones().append(RgbZone(QStringLiteral("ARGB Header 1"), 12));
    mutableZones().append(RgbZone(QStringLiteral("ARGB Header 2"), 12));
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

} // namespace lumacore
