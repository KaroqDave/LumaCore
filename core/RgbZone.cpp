#include "core/RgbZone.h"

#include <utility>

namespace lumacore {

QString rgbZoneTypeToString(RgbZoneType type)
{
    switch (type) {
    case RgbZoneType::Motherboard:
        return QStringLiteral("Motherboard");
    case RgbZoneType::AddressableHeader:
        return QStringLiteral("Addressable Header");
    case RgbZoneType::Unknown:
        return QStringLiteral("Unknown");
    }

    return QStringLiteral("Unknown");
}

RgbZoneType rgbZoneTypeFromString(const QString& value)
{
    if (value.compare(QStringLiteral("Motherboard"), Qt::CaseInsensitive) == 0) {
        return RgbZoneType::Motherboard;
    }

    if (value.compare(QStringLiteral("Addressable Header"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("AddressableHeader"), Qt::CaseInsensitive) == 0) {
        return RgbZoneType::AddressableHeader;
    }

    return RgbZoneType::Unknown;
}

RgbZone::RgbZone(QString name, RgbZoneType type, int ledCount, RgbColor initialColor)
    : m_name(std::move(name))
    , m_type(type)
    , m_currentColor(initialColor)
{
    m_leds.reserve(qMax(0, ledCount));

    for (int index = 0; index < ledCount; ++index) {
        m_leds.append(RgbLed(index, initialColor));
    }
}

const QString& RgbZone::name() const
{
    return m_name;
}

RgbZoneType RgbZone::type() const
{
    return m_type;
}

QString RgbZone::typeName() const
{
    return rgbZoneTypeToString(m_type);
}

int RgbZone::ledCount() const
{
    return static_cast<int>(m_leds.size());
}

const QVector<RgbLed>& RgbZone::leds() const
{
    return m_leds;
}

const RgbColor& RgbZone::currentColor() const
{
    return m_currentColor;
}

const RgbEffect& RgbZone::effect() const
{
    return m_effect;
}

void RgbZone::setName(QString name)
{
    m_name = std::move(name);
}

void RgbZone::setLedCount(int ledCount)
{
    const int sanitizedCount = qMax(0, ledCount);
    const int currentCount = m_leds.size();
    if (sanitizedCount == currentCount) {
        return;
    }

    if (sanitizedCount < currentCount) {
        m_leds.resize(sanitizedCount);
        return;
    }

    m_leds.reserve(sanitizedCount);
    for (int index = currentCount; index < sanitizedCount; ++index) {
        m_leds.append(RgbLed(index, m_currentColor));
    }
}

void RgbZone::setColor(const RgbColor& color)
{
    m_currentColor = color;

    for (RgbLed& led : m_leds) {
        led.setColor(color);
    }
}

void RgbZone::setLedColors(const QVector<RgbColor>& colors)
{
    if (colors.isEmpty()) {
        return;
    }

    const int count = m_leds.size();
    for (int index = 0; index < count; ++index) {
        m_leds[index].setColor(colors.at(index % colors.size()));
    }

    m_currentColor = colors.at(colors.size() / 2);
}

void RgbZone::setEffect(const RgbEffect& effect)
{
    m_effect = effect;
}

} // namespace lumacore
