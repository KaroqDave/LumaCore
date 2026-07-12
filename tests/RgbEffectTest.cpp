// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/RgbEffect.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    using namespace lumacore;

    const QVector<RgbEffectType> allTypes = allRgbEffectTypes();
    if (!require(allTypes.size() == 7, "the effect enumeration should list all seven effect types")) {
        return 1;
    }
    if (!require(
            allTypes.at(4) == RgbEffectType::Wave
                && allTypes.at(5) == RgbEffectType::Marquee
                && allTypes.at(6) == RgbEffectType::Strobe,
            "new effect types must append after the parity-frozen ColorCycle entry"
        )) {
        return 1;
    }

    for (const RgbEffectType type : allTypes) {
        if (!require(
                rgbEffectTypeFromString(rgbEffectTypeToString(type)) == type,
                "every effect type should round-trip through its string name"
            )) {
            return 1;
        }
        if (!require(
                isValidRgbEffectType(static_cast<int>(type)),
                "every enumerated effect type should be valid"
            )) {
            return 1;
        }
    }

    if (!require(!isValidRgbEffectType(allTypes.size()), "effect types past the enumeration should be invalid")) {
        return 1;
    }
    if (!require(!isValidRgbEffectType(-1), "negative effect types should be invalid")) {
        return 1;
    }
    if (!require(
            isAnimatedRgbEffectType(static_cast<int>(RgbEffectType::Wave))
                && isAnimatedRgbEffectType(static_cast<int>(RgbEffectType::Marquee))
                && isAnimatedRgbEffectType(static_cast<int>(RgbEffectType::Strobe))
                && !isAnimatedRgbEffectType(static_cast<int>(RgbEffectType::Static)),
            "the new effect types should count as animated"
        )) {
        return 1;
    }
    if (!require(
            rgbEffectTypeFromString(QStringLiteral("NotAnEffect")) == RgbEffectType::Static,
            "unknown effect names should fall back to Static"
        )) {
        return 1;
    }

    const RgbEffect wave(RgbEffectType::Wave, RgbColor(10, 20, 30), 2.5, 60);
    const RgbEffect restored = RgbEffect::fromJson(wave.toJson());
    if (!require(restored == wave, "effects should round-trip through profile JSON")) {
        return 1;
    }

    return 0;
}
