#include "core/RgbZone.h"

#include <utility>

namespace lumacore {

RgbZone::RgbZone(QString name, int ledCount, RgbColor initialColor)
    : m_name(std::move(name))
    , m_currentColor(initialColor)
{
    m_leds.reserve(qMax(0, ledCount));

    for (int index = 0; index < ledCount; ++index) {
        m_leds.append(RgbLed(QStringLiteral("LED %1").arg(index + 1), initialColor));
    }
}

const QString& RgbZone::name() const
{
    return m_name;
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

void RgbZone::setColor(const RgbColor& color)
{
    m_currentColor = color;

    for (RgbLed& led : m_leds) {
        led.setColor(color);
    }
}

} // namespace lumacore
