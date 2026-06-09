#pragma once

#include "core/RgbColor.h"
#include "core/RgbLed.h"

#include <QString>
#include <QVector>

namespace lumacore {

class RgbZone
{
public:
    RgbZone() = default;
    RgbZone(QString name, int ledCount, RgbColor initialColor = {});

    [[nodiscard]] const QString& name() const;
    [[nodiscard]] int ledCount() const;
    [[nodiscard]] const QVector<RgbLed>& leds() const;
    [[nodiscard]] const RgbColor& currentColor() const;

    void setColor(const RgbColor& color);

private:
    QString m_name;
    QVector<RgbLed> m_leds;
    RgbColor m_currentColor;
};

} // namespace lumacore
