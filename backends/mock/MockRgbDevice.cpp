#include "backends/mock/MockRgbDevice.h"

#include <QDebug>

namespace lumacore {

MockRgbDevice::MockRgbDevice(QObject* parent)
    : RgbDevice(
          QStringLiteral("mock-asus-tuf-x870-plus-wifi"),
          QStringLiteral("Mock ASUS TUF X870-PLUS WIFI"),
          QStringLiteral("ASUS"),
          RgbDeviceType::Motherboard,
          parent
      )
{
    mutableZones().append(RgbZone(QStringLiteral("Header 1"), RgbZoneType::Motherboard, 10));
    mutableZones().append(RgbZone(QStringLiteral("Header 2"), RgbZoneType::AddressableHeader, 30));
    mutableZones().append(RgbZone(QStringLiteral("Header 3"), RgbZoneType::AddressableHeader, 30));
}

bool MockRgbDevice::setZoneStaticColor(int zoneIndex, const RgbColor& color)
{
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }

    mutableZones()[zoneIndex].setColor(color);
    qInfo().noquote() << QStringLiteral("[MockBackend] %1 / %2 set to %3")
                             .arg(name(), zones().at(zoneIndex).name(), color.toHexString());
    emit zoneChanged(zoneIndex);
    return true;
}

} // namespace lumacore
