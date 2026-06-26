// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbColor.h"
#include "core/RgbEffect.h"
#include "core/RgbLed.h"

#include <QString>
#include <QVector>

namespace lumacore {

enum class RgbZoneType {
    Motherboard,
    AddressableHeader,
    Unknown
};

[[nodiscard]] QString rgbZoneTypeToString(RgbZoneType type);
[[nodiscard]] RgbZoneType rgbZoneTypeFromString(const QString& value);

class RgbZone
{
public:
    RgbZone() = default;
    RgbZone(QString name, RgbZoneType type, int ledCount, RgbColor initialColor = {});

    [[nodiscard]] const QString& name() const;
    [[nodiscard]] RgbZoneType type() const;
    [[nodiscard]] QString typeName() const;
    [[nodiscard]] int ledCount() const;
    [[nodiscard]] const QVector<RgbLed>& leds() const;
    [[nodiscard]] const RgbColor& currentColor() const;
    [[nodiscard]] const RgbEffect& effect() const;

    void setName(QString name);
    void setLedCount(int ledCount);
    void setColor(const RgbColor& color);
    void setLedColors(const QVector<RgbColor>& colors);
    void setEffect(const RgbEffect& effect);

private:
    QString m_name;
    RgbZoneType m_type {RgbZoneType::Unknown};
    QVector<RgbLed> m_leds;
    RgbColor m_currentColor;
    RgbEffect m_effect;
};

} // namespace lumacore
