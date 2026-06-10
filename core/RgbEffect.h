#pragma once

#include "core/RgbColor.h"

#include <QJsonObject>
#include <QString>

namespace lumacore {

enum class RgbEffectType {
    Static,
    Rainbow,
    Breathing
};

[[nodiscard]] QString rgbEffectTypeToString(RgbEffectType type);
[[nodiscard]] RgbEffectType rgbEffectTypeFromString(const QString& value);

class RgbEffect
{
public:
    RgbEffect() = default;
    RgbEffect(RgbEffectType type, RgbColor color, double speed = 1.0, int brightness = 100);

    [[nodiscard]] RgbEffectType type() const { return m_type; }
    [[nodiscard]] const RgbColor& color() const { return m_color; }
    [[nodiscard]] double speed() const { return m_speed; }
    [[nodiscard]] int brightness() const { return m_brightness; }
    [[nodiscard]] bool isAnimated() const { return m_type != RgbEffectType::Static; }

    void setType(RgbEffectType type);
    void setColor(const RgbColor& color);
    void setSpeed(double speed);
    void setBrightness(int brightness);

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static RgbEffect fromJson(const QJsonObject& object);

    [[nodiscard]] friend bool operator==(const RgbEffect&, const RgbEffect&) = default;

private:
    RgbEffectType m_type {RgbEffectType::Static};
    RgbColor m_color {};
    double m_speed {1.0};
    int m_brightness {100};
};

} // namespace lumacore
