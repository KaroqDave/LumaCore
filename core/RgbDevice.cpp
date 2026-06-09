#include "core/RgbDevice.h"

#include <utility>

namespace lumacore {

RgbDevice::RgbDevice(QString id, QString name, QObject* parent)
    : QObject(parent)
    , m_id(std::move(id))
    , m_name(std::move(name))
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

const QVector<RgbZone>& RgbDevice::zones() const
{
    return m_zones;
}

QVector<RgbZone>& RgbDevice::mutableZones()
{
    return m_zones;
}

} // namespace lumacore
