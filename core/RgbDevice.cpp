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

QVector<RgbZone>& RgbDevice::mutableZones()
{
    return m_zones;
}

} // namespace lumacore
