#pragma once

#include <QString>

namespace lumacore {

class RgbDevice;

class ZoneEffectPreferenceStore
{
public:
    [[nodiscard]] bool enabled(const RgbDevice& device, int zoneIndex) const;
    void setEnabled(const RgbDevice& device, int zoneIndex, bool enabled);

private:
    [[nodiscard]] static QString settingsKey(const RgbDevice& device, int zoneIndex);
};

} // namespace lumacore
