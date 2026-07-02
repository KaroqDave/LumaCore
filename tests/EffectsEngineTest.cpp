// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/EffectsEngine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

bool fuzzyEqual(double actual, double expected)
{
    return qAbs(actual - expected) < 0.05;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    using namespace lumacore;

    if (!require(
            fuzzyEqual(EffectsEngine::streamedEffectPeriodSeconds(0.1), 16.5),
            "minimum speed should match the LumaScope slow rainbow cadence"
        )) {
        return 1;
    }
    if (!require(
            fuzzyEqual(EffectsEngine::streamedEffectPeriodSeconds(5.0), 1.6),
            "maximum speed should match the LumaScope fast rainbow cadence"
        )) {
        return 1;
    }
    if (!require(
            fuzzyEqual(EffectsEngine::streamedEffectPeriodSeconds(1.0), 2.82),
            "1x speed should land near the captured mid-speed period"
        )) {
        return 1;
    }

    const RgbEffect cycle(RgbEffectType::ColorCycle, RgbColor(255, 0, 0), 5.0, 100);
    const QVector<RgbColor> start = EffectsEngine::computeFrame(cycle, 1, 0.0);
    const QVector<RgbColor> wrapped = EffectsEngine::computeFrame(
        cycle,
        1,
        EffectsEngine::streamedEffectPeriodSeconds(cycle.speed())
    );
    if (!require(start.size() == 1 && wrapped.size() == 1, "color-cycle frames should render one LED")) {
        return 1;
    }
    if (!require(start.first() == wrapped.first(), "color-cycle frames should wrap after one streamed period")) {
        return 1;
    }

    const RgbEffect rainbow(RgbEffectType::Rainbow, RgbColor(255, 255, 255), 1.0, 50);
    const QVector<RgbColor> rainbowFrame = EffectsEngine::computeFrame(rainbow, 4, 0.0);
    if (!require(rainbowFrame.size() == 4, "rainbow should render one color per LED")) {
        return 1;
    }
    if (!require(
            rainbowFrame.first().red() <= 128,
            "rainbow brightness should scale rendered frame colors"
        )) {
        return 1;
    }

    return 0;
}
