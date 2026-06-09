#pragma once

#include "core/RgbColor.h"

#include <QString>

namespace lumacore {

class RgbLed
{
public:
    RgbLed() = default;
    explicit RgbLed(QString name, RgbColor color = {});

    [[nodiscard]] const QString& name() const;
    [[nodiscard]] const RgbColor& color() const;

    void setColor(const RgbColor& color);

private:
    QString m_name;
    RgbColor m_color;
};

} // namespace lumacore
