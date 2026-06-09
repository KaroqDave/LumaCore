#include "core/RgbLed.h"

#include <utility>

namespace lumacore {

RgbLed::RgbLed(QString name, RgbColor color)
    : m_name(std::move(name))
    , m_color(color)
{
}

const QString& RgbLed::name() const
{
    return m_name;
}

const RgbColor& RgbLed::color() const
{
    return m_color;
}

void RgbLed::setColor(const RgbColor& color)
{
    m_color = color;
}

} // namespace lumacore
