#include "core/RgbDevice.h"

#include <utility>

namespace lumacore {

QString rgbDeviceTypeToString(RgbDeviceType type)
{
    switch (type) {
    case RgbDeviceType::Motherboard:
        return QStringLiteral("Motherboard");
    case RgbDeviceType::Controller:
        return QStringLiteral("Controller");
    case RgbDeviceType::Unknown:
        return QStringLiteral("Unknown");
    }

    return QStringLiteral("Unknown");
}

RgbDeviceType rgbDeviceTypeFromString(const QString& value)
{
    if (value.compare(QStringLiteral("Motherboard"), Qt::CaseInsensitive) == 0) {
        return RgbDeviceType::Motherboard;
    }

    if (value.compare(QStringLiteral("Controller"), Qt::CaseInsensitive) == 0) {
        return RgbDeviceType::Controller;
    }

    return RgbDeviceType::Unknown;
}

RgbDevice::RgbDevice(QString id, QString name, QString vendor, RgbDeviceType type, QObject* parent)
    : QObject(parent)
    , m_id(std::move(id))
    , m_name(std::move(name))
    , m_vendor(std::move(vendor))
    , m_type(type)
{
}

const QString& RgbDevice::id() const
{
    return m_id;
}

const QString& RgbDevice::name() const
{
    return m_name;
}

const QString& RgbDevice::vendor() const
{
    return m_vendor;
}

RgbDeviceType RgbDevice::type() const
{
    return m_type;
}

QString RgbDevice::typeName() const
{
    return rgbDeviceTypeToString(m_type);
}

const QVector<RgbZone>& RgbDevice::zones() const
{
    return m_zones;
}

bool RgbDevice::setZoneName(int zoneIndex, const QString& name)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size()) {
        return false;
    }

    const QString sanitizedName = name.trimmed();
    if (sanitizedName.isEmpty() || m_zones.at(zoneIndex).name() == sanitizedName) {
        return false;
    }

    m_zones[zoneIndex].setName(sanitizedName);
    emit zoneChanged(zoneIndex);
    return true;
}

bool RgbDevice::setZoneLedCount(int zoneIndex, int ledCount)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size() || ledCount < 0 || m_zones.at(zoneIndex).ledCount() == ledCount) {
        return false;
    }

    m_zones[zoneIndex].setLedCount(ledCount);
    emit zoneChanged(zoneIndex);
    return true;
}

QVector<RgbZone>& RgbDevice::mutableZones()
{
    return m_zones;
}

} // namespace lumacore
