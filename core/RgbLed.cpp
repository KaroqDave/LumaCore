#include "core/RgbLed.h"

namespace lumacore {

RgbLed::RgbLed(int index, RgbColor color)
    : m_index(index)
    , m_color(color)
{
}

int RgbLed::index() const
{
    return m_index;
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
