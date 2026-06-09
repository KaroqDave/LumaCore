#pragma once

#include "core/RgbColor.h"

namespace lumacore {

class RgbLed
{
public:
    RgbLed() = default;
    explicit RgbLed(int index, RgbColor color = {});

    [[nodiscard]] int index() const;
    [[nodiscard]] const RgbColor& color() const;

    void setColor(const RgbColor& color);

private:
    int m_index {0};
    RgbColor m_color;
};

} // namespace lumacore
