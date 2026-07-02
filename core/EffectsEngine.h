// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbColor.h"
#include "core/RgbEffect.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QTimer>
#include <QVector>

namespace lumacore {

class DeviceManager;

class EffectsEngine : public QObject
{
    Q_OBJECT

public:
    explicit EffectsEngine(DeviceManager* deviceManager, QObject* parent = nullptr);

    void startZone(int deviceIndex, int zoneIndex);
    void stopZone(int deviceIndex, int zoneIndex);
    void stopAll();

    [[nodiscard]] static double streamedEffectPeriodSeconds(double speed);
    [[nodiscard]] static QVector<RgbColor> computeFrame(const RgbEffect& effect, int ledCount, double elapsedSeconds);

private:
    void tick();
    void updateTimerState();

    DeviceManager* m_deviceManager {nullptr};
    QTimer m_timer;
    QElapsedTimer m_clock;
    QSet<QPair<int, int>> m_activeZones;
};

} // namespace lumacore
