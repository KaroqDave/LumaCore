#pragma once

#include "core/RgbDevice.h"
#include "hardware/linux/ProbeResult.h"

namespace lumacore {

class LinuxDiscoveredDevice final : public RgbDevice
{
    Q_OBJECT

public:
    explicit LinuxDiscoveredDevice(const hardware::linux::ProbeDevice& device, QObject* parent = nullptr);

    [[nodiscard]] QString discoveryIdentity() const override;
    [[nodiscard]] const QString& source() const;
    [[nodiscard]] const QString& path() const;
    [[nodiscard]] const QString& details() const;

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override;
    [[nodiscard]] bool applyZoneEffect(int zoneIndex, const RgbEffect& effect) override;
    [[nodiscard]] bool applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors) override;
    [[nodiscard]] bool updateZoneMetadata(int zoneIndex, const QString& name, int ledCount) override;
    [[nodiscard]] bool usesLocalFrameRendering() const override;
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability capability) const override;

private:
    QString m_discoveryIdentity;
    QString m_source;
    QString m_path;
    QString m_details;
};

} // namespace lumacore
