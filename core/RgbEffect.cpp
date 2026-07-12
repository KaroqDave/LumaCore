// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/RgbEffect.h"

#include <QtGlobal>

namespace lumacore {

namespace {

constexpr double kMinSpeed = 0.1;
constexpr double kMaxSpeed = 5.0;

} // namespace

QString rgbEffectTypeToString(RgbEffectType type)
{
    switch (type) {
    case RgbEffectType::Static:
        return QStringLiteral("Static");
    case RgbEffectType::Rainbow:
        return QStringLiteral("Rainbow");
    case RgbEffectType::Breathing:
        return QStringLiteral("Breathing");
    case RgbEffectType::ColorCycle:
        return QStringLiteral("ColorCycle");
    case RgbEffectType::Wave:
        return QStringLiteral("Wave");
    case RgbEffectType::Marquee:
        return QStringLiteral("Marquee");
    case RgbEffectType::Strobe:
        return QStringLiteral("Strobe");
    }

    return QStringLiteral("Static");
}

RgbEffectType rgbEffectTypeFromString(const QString& value)
{
    if (value.compare(QStringLiteral("Rainbow"), Qt::CaseInsensitive) == 0) {
        return RgbEffectType::Rainbow;
    }

    if (value.compare(QStringLiteral("Breathing"), Qt::CaseInsensitive) == 0) {
        return RgbEffectType::Breathing;
    }

    if (value.compare(QStringLiteral("ColorCycle"), Qt::CaseInsensitive) == 0) {
        return RgbEffectType::ColorCycle;
    }

    if (value.compare(QStringLiteral("Wave"), Qt::CaseInsensitive) == 0) {
        return RgbEffectType::Wave;
    }

    if (value.compare(QStringLiteral("Marquee"), Qt::CaseInsensitive) == 0) {
        return RgbEffectType::Marquee;
    }

    if (value.compare(QStringLiteral("Strobe"), Qt::CaseInsensitive) == 0) {
        return RgbEffectType::Strobe;
    }

    return RgbEffectType::Static;
}

QVector<RgbEffectType> allRgbEffectTypes()
{
    return {
        RgbEffectType::Static,
        RgbEffectType::Rainbow,
        RgbEffectType::Breathing,
        RgbEffectType::ColorCycle,
        RgbEffectType::Wave,
        RgbEffectType::Marquee,
        RgbEffectType::Strobe,
    };
}

bool isValidRgbEffectType(int effectType)
{
    for (const RgbEffectType knownEffect : allRgbEffectTypes()) {
        if (effectType == static_cast<int>(knownEffect)) {
            return true;
        }
    }
    return false;
}

bool isAnimatedRgbEffectType(int effectType)
{
    return isValidRgbEffectType(effectType)
        && effectType != static_cast<int>(RgbEffectType::Static);
}

RgbEffect::RgbEffect(RgbEffectType type, RgbColor color, double speed, int brightness)
    : m_type(type)
    , m_color(color)
{
    setSpeed(speed);
    setBrightness(brightness);
}

void RgbEffect::setType(RgbEffectType type)
{
    m_type = type;
}

void RgbEffect::setColor(const RgbColor& color)
{
    m_color = color;
}

void RgbEffect::setSpeed(double speed)
{
    m_speed = qBound(kMinSpeed, speed, kMaxSpeed);
}

void RgbEffect::setBrightness(int brightness)
{
    m_brightness = qBound(0, brightness, 100);
}

QJsonObject RgbEffect::toJson() const
{
    return {
        {QStringLiteral("type"), rgbEffectTypeToString(m_type)},
        {QStringLiteral("color"), m_color.toHexString()},
        {QStringLiteral("speed"), m_speed},
        {QStringLiteral("brightness"), m_brightness},
    };
}

RgbEffect RgbEffect::fromJson(const QJsonObject& object)
{
    const RgbEffectType type = rgbEffectTypeFromString(object.value(QStringLiteral("type")).toString());
    bool colorOk = false;
    const RgbColor color = RgbColor::fromHexString(object.value(QStringLiteral("color")).toString(), &colorOk);
    const double speed = object.value(QStringLiteral("speed")).toDouble(1.0);
    const int brightness = object.value(QStringLiteral("brightness")).toInt(100);

    return RgbEffect(type, colorOk ? color : RgbColor(), speed, brightness);
}

} // namespace lumacore
