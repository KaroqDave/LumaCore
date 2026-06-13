#include "core/EffectsEngine.h"

#include "core/DeviceManager.h"
#include "core/RgbDevice.h"

#include <QColor>
#include <QtGlobal>
#include <QtMath>

namespace lumacore {

namespace {

constexpr int kFrameIntervalMs = 16; // ~60 Hz
constexpr double kRainbowPeriodSeconds = 6.0;
constexpr double kColorCyclePeriodSeconds = 6.0;
constexpr double kBreathingPeriodSeconds = 4.0;
constexpr double kBreathingFloor = 0.12;

[[nodiscard]] double fractional(double value)
{
    return value - std::floor(value);
}

} // namespace

EffectsEngine::EffectsEngine(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
{
    m_timer.setInterval(kFrameIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &EffectsEngine::tick);
}

void EffectsEngine::startZone(int deviceIndex, int zoneIndex)
{
    m_activeZones.insert({deviceIndex, zoneIndex});
    updateTimerState();
}

void EffectsEngine::stopZone(int deviceIndex, int zoneIndex)
{
    m_activeZones.remove({deviceIndex, zoneIndex});
    updateTimerState();
}

void EffectsEngine::stopAll()
{
    m_activeZones.clear();
    updateTimerState();
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

    for (const QPair<int, int>& zoneKey : m_activeZones) {
        const RgbDevice* device = m_deviceManager->deviceAt(zoneKey.first);
        if (device == nullptr || zoneKey.second < 0 || zoneKey.second >= device->zones().size()) {
            continue;
        }

        const RgbZone& zone = device->zones().at(zoneKey.second);
        const QVector<RgbColor> frame = computeFrame(zone.effect(), zone.ledCount(), elapsedSeconds);
        if (frame.isEmpty()) {
            continue;
        }

        m_deviceManager->paintZoneFrame(zoneKey.first, zoneKey.second, frame);
    }
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
        const double offset = elapsedSeconds * effect.speed() / kRainbowPeriodSeconds;
        for (int index = 0; index < ledCount; ++index) {
            const double hue = fractional(offset + static_cast<double>(index) / static_cast<double>(ledCount));
            const QColor color = QColor::fromHsvF(static_cast<float>(hue), 1.0F, static_cast<float>(brightness));
            colors.append(RgbColor::fromQColor(color));
        }
        break;
    }
    case RgbEffectType::ColorCycle: {
        const double hue = fractional(elapsedSeconds * effect.speed() / kColorCyclePeriodSeconds);
        const QColor color = QColor::fromHsvF(static_cast<float>(hue), 1.0F, static_cast<float>(brightness));
        colors.fill(RgbColor::fromQColor(color), ledCount);
        break;
    }
    case RgbEffectType::Breathing: {
        const double phase = elapsedSeconds * effect.speed() / kBreathingPeriodSeconds;
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
