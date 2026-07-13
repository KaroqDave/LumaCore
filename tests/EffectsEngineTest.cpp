// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/EffectsEngine.h"
#include "core/DeviceManager.h"
#include "core/RgbDevice.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <QtGlobal>

#include <functional>
#include <memory>

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

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 1000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

class CountingFrameDevice final : public lumacore::RgbDevice
{
public:
    CountingFrameDevice()
        : RgbDevice(
              QStringLiteral("counting-frame-device"),
              QStringLiteral("Counting Frame Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Zone"),
            lumacore::RgbZoneType::AddressableHeader,
            3
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        if (zoneIndex < 0 || zoneIndex >= mutableZones().size()) {
            return false;
        }
        mutableZones()[zoneIndex].setColor(color);
        return true;
    }

    [[nodiscard]] bool applyZoneFrame(
        int zoneIndex,
        const QVector<lumacore::RgbColor>& colors
    ) override
    {
        ++frameAttempts;
        if (!acceptFrames) {
            return false;
        }
        return RgbDevice::applyZoneFrame(zoneIndex, colors);
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneColorWrite
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(
        lumacore::BackendCapability capability
    ) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported test capability.")};
    }

    int frameAttempts {0};
    bool acceptFrames {true};
};

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

    DeviceManager manager;
    manager.setDryRunEnabled(false);
    auto countingDevice = std::make_unique<CountingFrameDevice>();
    CountingFrameDevice* device = countingDevice.get();
    device->setZoneEffect(0, RgbEffect(RgbEffectType::Strobe, RgbColor(255, 255, 255), 0.1, 100));
    std::vector<std::unique_ptr<RgbDevice>> devices;
    devices.push_back(std::move(countingDevice));
    manager.replaceDevices(std::move(devices));

    manager.startZoneFrameStreaming(0, 0);
    if (!require(waitUntil([device] { return device->frameAttempts >= 1; }), "streaming should submit an initial frame")) {
        return 1;
    }
    QElapsedTimer stableFrameTimer;
    stableFrameTimer.start();
    while (stableFrameTimer.elapsed() < 100) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    if (!require(device->frameAttempts == 1, "unchanged streamed frames should be deduplicated")) {
        return 1;
    }

    manager.stopZoneFrameStreaming(0, 0);
    manager.startZoneFrameStreaming(0, 0);
    if (!require(
            waitUntil([device] { return device->frameAttempts >= 2; }),
            "restarting a zone should invalidate its cached frame"
        )) {
        return 1;
    }

    manager.stopDeviceFrameStreaming(0);
    manager.startZoneFrameStreaming(0, 0);
    if (!require(
            waitUntil([device] { return device->frameAttempts >= 3; }),
            "stopping a device should invalidate its cached frames"
        )) {
        return 1;
    }

    manager.stopAllFrameStreaming();
    manager.startZoneFrameStreaming(0, 0);
    if (!require(
            waitUntil([device] { return device->frameAttempts >= 4; }),
            "stopping all streams should invalidate cached frames"
        )) {
        return 1;
    }

    manager.stopZoneFrameStreaming(0, 0);
    device->setZoneEffect(0, RgbEffect(RgbEffectType::ColorCycle, RgbColor(255, 255, 255), 5.0, 100));
    const int attemptsBeforeChangingFrames = device->frameAttempts;
    manager.startZoneFrameStreaming(0, 0);
    if (!require(
            waitUntil([device, attemptsBeforeChangingFrames] {
                return device->frameAttempts >= attemptsBeforeChangingFrames + 2;
            }),
            "changed streamed frames should still be submitted"
        )) {
        return 1;
    }

    manager.stopZoneFrameStreaming(0, 0);
    device->setZoneEffect(0, RgbEffect(RgbEffectType::Strobe, RgbColor(255, 255, 255), 0.1, 100));
    device->acceptFrames = false;
    const int attemptsBeforeRejection = device->frameAttempts;
    manager.startZoneFrameStreaming(0, 0);
    if (!require(
            waitUntil([device, attemptsBeforeRejection] {
                return device->frameAttempts >= attemptsBeforeRejection + 2;
            }),
            "rejected frames should not be cached"
        )) {
        return 1;
    }
    manager.stopAllFrameStreaming();

    return 0;
}
