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
    // Modern blue-ish defaults so previews and zone swatches are inviting out of the box.
    const RgbColor header1Color(77, 141, 255);   // #4D8DFF
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
    qInfo().noquote() << QStringLiteral("[MockBackend] %1 / %2 set to %3")
                             .arg(name(), zones().at(zoneIndex).name(), color.toHexString());
    emit zoneChanged(zoneIndex);
    return true;
}

} // namespace lumacore
