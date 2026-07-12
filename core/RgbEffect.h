// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbColor.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace lumacore {

enum class RgbEffectType {
    Static,
    Rainbow,
    Breathing,
    ColorCycle,
    // Values append only: stored profiles and the daemon protocol carry these
    // as stable integers/names (docs/refactor-parity.md).
    Wave,
    Marquee,
    Strobe
};

[[nodiscard]] QString rgbEffectTypeToString(RgbEffectType type);
[[nodiscard]] RgbEffectType rgbEffectTypeFromString(const QString& value);
[[nodiscard]] QVector<RgbEffectType> allRgbEffectTypes();
[[nodiscard]] bool isValidRgbEffectType(int effectType);
[[nodiscard]] bool isAnimatedRgbEffectType(int effectType);

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
