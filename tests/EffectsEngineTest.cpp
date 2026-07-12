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

    const RgbEffect wave(RgbEffectType::Wave, RgbColor(255, 0, 0), 1.0, 100);
    const double wavePeriod = EffectsEngine::streamedEffectPeriodSeconds(wave.speed());
    const QVector<RgbColor> waveFrame = EffectsEngine::computeFrame(wave, 8, 0.0);
    if (!require(waveFrame.size() == 8, "wave should render one color per LED")) {
        return 1;
    }
    int waveBrightest = 0;
    int waveDarkest = 255;
    for (const RgbColor& color : waveFrame) {
        waveBrightest = qMax(waveBrightest, static_cast<int>(color.red()));
        waveDarkest = qMin(waveDarkest, static_cast<int>(color.red()));
    }
    if (!require(
            waveBrightest > 200 && waveDarkest < 55,
            "wave should render a crest and a trough across the strip"
        )) {
        return 1;
    }
    if (!require(
            waveFrame.first().green() == 0 && waveFrame.first().blue() == 0,
            "wave should keep the base color hue"
        )) {
        return 1;
    }
    const QVector<RgbColor> waveWrapped = EffectsEngine::computeFrame(wave, 8, wavePeriod);
    bool waveWraps = true;
    for (int index = 0; index < waveFrame.size(); ++index) {
        if (qAbs(waveFrame.at(index).red() - waveWrapped.at(index).red()) > 1) {
            waveWraps = false;
        }
    }
    if (!require(waveWraps, "wave frames should wrap after one streamed period")) {
        return 1;
    }

    const RgbEffect marquee(RgbEffectType::Marquee, RgbColor(0, 255, 0), 1.0, 100);
    const double marqueePeriod = EffectsEngine::streamedEffectPeriodSeconds(marquee.speed());
    const QVector<RgbColor> marqueeFrame = EffectsEngine::computeFrame(marquee, 12, 0.0);
    if (!require(marqueeFrame.size() == 12, "marquee should render one color per LED")) {
        return 1;
    }
    bool marqueeBlocks = true;
    for (int index = 0; index < 12; ++index) {
        const bool expectLit = (index % 6) < 3;
        const bool lit = marqueeFrame.at(index).green() == 255;
        const bool dark = marqueeFrame.at(index).green() == 0;
        if (expectLit ? !lit : !dark) {
            marqueeBlocks = false;
        }
    }
    if (!require(
            marqueeBlocks,
            "marquee should start with repeating three-on/three-off blocks"
        )) {
        return 1;
    }
    const QVector<RgbColor> marqueeShifted = EffectsEngine::computeFrame(marquee, 12, marqueePeriod / 2.0);
    if (!require(
            marqueeShifted.at(0).green() == 0 && marqueeShifted.at(3).green() == 255,
            "marquee blocks should scroll by half a pattern at half a period"
        )) {
        return 1;
    }
    if (!require(
            EffectsEngine::computeFrame(marquee, 12, marqueePeriod) == marqueeFrame,
            "marquee frames should wrap after one streamed period"
        )) {
        return 1;
    }

    const RgbEffect strobe(RgbEffectType::Strobe, RgbColor(0, 0, 255), 1.0, 100);
    const double strobePeriod = EffectsEngine::streamedEffectPeriodSeconds(strobe.speed());
    const QVector<RgbColor> strobeLit = EffectsEngine::computeFrame(strobe, 3, 0.2 * strobePeriod / 8.0);
    const QVector<RgbColor> strobeDark = EffectsEngine::computeFrame(strobe, 3, 0.6 * strobePeriod / 8.0);
    if (!require(
            strobeLit.size() == 3 && strobeDark.size() == 3,
            "strobe should render one color per LED"
        )) {
        return 1;
    }
    if (!require(
            strobeLit.first().blue() == 255 && strobeLit.first() == strobeLit.last(),
            "strobe should light the whole zone during the on phase"
        )) {
        return 1;
    }
    if (!require(
            strobeDark.first() == RgbColor(),
            "strobe should black out the whole zone during the off phase"
        )) {
        return 1;
    }

    return 0;
}
