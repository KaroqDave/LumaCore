#pragma once

#include "core/RgbDevice.h"
#include "hardware/linux/AsusAuraHidProtocol.h"
#include "hardware/linux/HidDeviceWriter.h"
#include "hardware/linux/ProbeResult.h"

namespace lumacore {

class AsusAuraHidDevice final : public RgbDevice
{
    Q_OBJECT

public:
    AsusAuraHidDevice(
        hardware::linux::ProbeDevice device,
        bool configTableVerified,
        hardware::linux::AsusAuraConfigTable configTable,
        QString configSummary,
        QObject* parent = nullptr
    );

    [[nodiscard]] QString discoveryIdentity() const override;
    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override;
    [[nodiscard]] bool applyZoneEffect(int zoneIndex, const RgbEffect& effect) override;
    [[nodiscard]] bool applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors) override;
    [[nodiscard]] bool applyAllOff() override;
    [[nodiscard]] bool updateZoneMetadata(int zoneIndex, const QString& name, int ledCount) override;
    [[nodiscard]] bool usesLocalFrameRendering() const override;
    [[nodiscard]] QString previewZoneEffectWrite(int zoneIndex, const RgbEffect& effect) const override;
    [[nodiscard]] QString lastHardwareWriteStatus() const override;
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability capability) const override;

private:
    void initializeZones();
    [[nodiscard]] QString writeDisabledReason() const;

    hardware::linux::ProbeDevice m_device;
    bool m_configTableVerified {false};
    hardware::linux::AsusAuraConfigTable m_configTable;
    QString m_configSummary;
    QString m_lastHardwareWriteStatus;
    hardware::linux::HidDeviceWriter m_writer;
};

} // namespace lumacore
