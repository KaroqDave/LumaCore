// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/EffectsEngine.h"

#include "core/DeviceManager.h"
#include "core/RgbDevice.h"

#include <QColor>
#include <QtGlobal>
#include <QtMath>

namespace lumacore {

namespace {

constexpr int kFrameIntervalMs = 16; // ~60 Hz
constexpr double kMinEffectSpeed = 0.1;
constexpr double kMaxEffectSpeed = 5.0;
constexpr double kVendorFastStreamPeriodSeconds = 1.6;
constexpr double kVendorSlowStreamPeriodSeconds = 16.5;
constexpr double kBreathingFloor = 0.12;
constexpr int kMarqueeBlockLeds = 3;
constexpr double kStrobeFlashesPerPeriod = 8.0;

[[nodiscard]] double fractional(double value)
{
    return value - std::floor(value);
}

[[nodiscard]] double streamPeriodScale()
{
    return (kVendorSlowStreamPeriodSeconds - kVendorFastStreamPeriodSeconds)
        / ((1.0 / kMinEffectSpeed) - (1.0 / kMaxEffectSpeed));
}

[[nodiscard]] double streamPeriodOffset()
{
    return kVendorFastStreamPeriodSeconds - (streamPeriodScale() / kMaxEffectSpeed);
}

} // namespace

EffectsEngine::EffectsEngine(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
{
    m_timer.setInterval(kFrameIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &EffectsEngine::tick);
    // The phase clock runs from construction so previews can animate before
    // the first streamed zone; streaming reuses the same monotonic clock.
    m_clock.start();
}

void EffectsEngine::startZone(int deviceIndex, int zoneIndex)
{
    const QPair<int, int> zoneKey {deviceIndex, zoneIndex};
    m_lastAcceptedFrames.remove(zoneKey);
    m_activeZones.insert(zoneKey);
    updateTimerState();
}

void EffectsEngine::stopZone(int deviceIndex, int zoneIndex)
{
    const QPair<int, int> zoneKey {deviceIndex, zoneIndex};
    m_activeZones.remove(zoneKey);
    m_lastAcceptedFrames.remove(zoneKey);
    updateTimerState();
}

void EffectsEngine::stopDevice(int deviceIndex)
{
    // Remove every streaming zone for the device by device index rather than by
    // its current zone count, so a zone that outlived a metadata edit / device
    // replace (an index no longer in zones()) is still stopped.
    QSet<QPair<int, int>> remaining;
    QHash<QPair<int, int>, QVector<RgbColor>> remainingFrames;
    for (const QPair<int, int>& zoneKey : m_activeZones) {
        if (zoneKey.first != deviceIndex) {
            remaining.insert(zoneKey);
            if (m_lastAcceptedFrames.contains(zoneKey)) {
                remainingFrames.insert(zoneKey, m_lastAcceptedFrames.value(zoneKey));
            }
        }
    }
    m_activeZones = remaining;
    m_lastAcceptedFrames = remainingFrames;
    updateTimerState();
}

void EffectsEngine::stopAll()
{
    m_activeZones.clear();
    m_lastAcceptedFrames.clear();
    updateTimerState();
}

bool EffectsEngine::isZoneStreaming(int deviceIndex, int zoneIndex) const
{
    return m_activeZones.contains({deviceIndex, zoneIndex});
}

void EffectsEngine::updateTimerState()
{
    if (m_activeZones.isEmpty()) {
        m_timer.stop();
        return;
    }

    if (!m_timer.isActive()) {
        if (!m_clock.isValid()) {
            m_clock.start();
        }
        m_timer.start();
    }
}

void EffectsEngine::tick()
{
    if (m_deviceManager == nullptr || m_activeZones.isEmpty()) {
        return;
    }

    const double elapsedSeconds = static_cast<double>(m_clock.elapsed()) / 1000.0;

    const QSet<QPair<int, int>> activeZones = m_activeZones;
    for (const QPair<int, int>& zoneKey : activeZones) {
        if (!m_activeZones.contains(zoneKey)) {
            continue;
        }

        const RgbDevice* device = m_deviceManager->deviceAt(zoneKey.first);
        if (device == nullptr || zoneKey.second < 0 || zoneKey.second >= device->zones().size()) {
            continue;
        }

        const RgbZone& zone = device->zones().at(zoneKey.second);
        const QVector<RgbColor> frame = computeFrame(zone.effect(), zone.ledCount(), elapsedSeconds);
        if (frame.isEmpty()) {
            continue;
        }

        if (m_lastAcceptedFrames.value(zoneKey) == frame) {
            continue;
        }

        if (m_deviceManager->paintZoneFrame(zoneKey.first, zoneKey.second, frame)) {
            m_lastAcceptedFrames.insert(zoneKey, frame);
        }
    }
}

double EffectsEngine::streamedEffectPeriodSeconds(double speed)
{
    const double boundedSpeed = qBound(kMinEffectSpeed, speed, kMaxEffectSpeed);
    return streamPeriodOffset() + (streamPeriodScale() / boundedSpeed);
}

double EffectsEngine::streamPhase(double speed) const
{
    if (!m_clock.isValid()) {
        return 0.0;
    }

    const double elapsedSeconds = static_cast<double>(m_clock.elapsed()) / 1000.0;
    return fractional(elapsedSeconds / streamedEffectPeriodSeconds(speed));
}

QVector<RgbColor> EffectsEngine::computeFrame(const RgbEffect& effect, int ledCount, double elapsedSeconds)
{
    if (ledCount <= 0) {
        return {};
    }

    const double brightness = qBound(0.0, static_cast<double>(effect.brightness()) / 100.0, 1.0);
    QVector<RgbColor> colors;
    colors.reserve(ledCount);

    switch (effect.type()) {
    case RgbEffectType::Rainbow: {
        const double offset = elapsedSeconds / streamedEffectPeriodSeconds(effect.speed());
        for (int index = 0; index < ledCount; ++index) {
            const double hue = fractional(offset + static_cast<double>(index) / static_cast<double>(ledCount));
            const QColor color = QColor::fromHsvF(static_cast<float>(hue), 1.0F, static_cast<float>(brightness));
            colors.append(RgbColor::fromQColor(color));
        }
        break;
    }
    case RgbEffectType::ColorCycle: {
        const double hue = fractional(elapsedSeconds / streamedEffectPeriodSeconds(effect.speed()));
        const QColor color = QColor::fromHsvF(static_cast<float>(hue), 1.0F, static_cast<float>(brightness));
        colors.fill(RgbColor::fromQColor(color), ledCount);
        break;
    }
    case RgbEffectType::Breathing: {
        const double phase = elapsedSeconds / streamedEffectPeriodSeconds(effect.speed());
        const double wave = (std::sin(phase * 2.0 * M_PI) + 1.0) / 2.0;
        const double factor = (kBreathingFloor + (1.0 - kBreathingFloor) * wave) * brightness;
        const RgbColor base = effect.color();
        const RgbColor scaled = RgbColor::fromRgb(
            static_cast<int>(std::lround(base.red() * factor)),
            static_cast<int>(std::lround(base.green() * factor)),
            static_cast<int>(std::lround(base.blue() * factor))
        );
        colors.fill(scaled, ledCount);
        break;
    }
    case RgbEffectType::Wave: {
        // One brightness crest of the base color travels the strip per period,
        // matching the rainbow convention of one pattern traversal per period.
        const double offset = elapsedSeconds / streamedEffectPeriodSeconds(effect.speed());
        const RgbColor base = effect.color();
        for (int index = 0; index < ledCount; ++index) {
            const double position = static_cast<double>(index) / static_cast<double>(ledCount);
            const double wave = (std::sin((position - offset) * 2.0 * M_PI) + 1.0) / 2.0;
            const double factor = wave * brightness;
            colors.append(RgbColor::fromRgb(
                static_cast<int>(std::lround(base.red() * factor)),
                static_cast<int>(std::lround(base.green() * factor)),
                static_cast<int>(std::lround(base.blue() * factor))
            ));
        }
        break;
    }
    case RgbEffectType::Marquee: {
        // Repeating lit/dark blocks scroll by one full block pair per period.
        const double offset = elapsedSeconds / streamedEffectPeriodSeconds(effect.speed());
        const int patternLeds = kMarqueeBlockLeds * 2;
        const int shift = static_cast<int>(std::floor(fractional(offset) * patternLeds));
        const RgbColor base = effect.color();
        const RgbColor lit = RgbColor::fromRgb(
            static_cast<int>(std::lround(base.red() * brightness)),
            static_cast<int>(std::lround(base.green() * brightness)),
            static_cast<int>(std::lround(base.blue() * brightness))
        );
        for (int index = 0; index < ledCount; ++index) {
            const int patternIndex = ((index - shift) % patternLeds + patternLeds) % patternLeds;
            colors.append(patternIndex < kMarqueeBlockLeds ? lit : RgbColor());
        }
        break;
    }
    case RgbEffectType::Strobe: {
        const double offset = elapsedSeconds / streamedEffectPeriodSeconds(effect.speed());
        const bool lit = fractional(offset * kStrobeFlashesPerPeriod) < 0.5;
        const RgbColor base = effect.color();
        const RgbColor scaled = lit
            ? RgbColor::fromRgb(
                  static_cast<int>(std::lround(base.red() * brightness)),
                  static_cast<int>(std::lround(base.green() * brightness)),
                  static_cast<int>(std::lround(base.blue() * brightness))
              )
            : RgbColor();
        colors.fill(scaled, ledCount);
        break;
    }
    case RgbEffectType::Static: {
        const RgbColor base = effect.color();
        const RgbColor scaled = RgbColor::fromRgb(
            static_cast<int>(std::lround(base.red() * brightness)),
            static_cast<int>(std::lround(base.green() * brightness)),
            static_cast<int>(std::lround(base.blue() * brightness))
        );
        colors.fill(scaled, ledCount);
        break;
    }
    }

    return colors;
}

} // namespace lumacore
