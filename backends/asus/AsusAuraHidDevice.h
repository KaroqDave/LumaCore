// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbDevice.h"
#include "backends/asus/AsusAuraHidPlatform.h"
#include "hardware/linux/AsusAuraHidProtocol.h"

namespace lumacore {

class AsusAuraHidDevice final : public RgbDevice
{
    Q_OBJECT

public:
    AsusAuraHidDevice(
        asus_aura_platform::ProbeDevice device,
        bool configTableVerified,
        hardware::linux::AsusAuraConfigTable configTable,
        QString configSummary,
        QObject* parent = nullptr
    );

    [[nodiscard]] QString discoveryIdentity() const override;
    [[nodiscard]] QString discoverySupportStage() const override;
    [[nodiscard]] QString discoverySupportStatus() const override;
    [[nodiscard]] QString discoverySupportFamily() const override;
    [[nodiscard]] QString discoverySupportNotes() const override;
    [[nodiscard]] bool discoveryCataloged() const override;
    [[nodiscard]] bool discoveryWriteCapableBackend() const override;
    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override;
    [[nodiscard]] bool applyZoneEffect(int zoneIndex, const RgbEffect& effect) override;
    [[nodiscard]] bool applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors) override;
    [[nodiscard]] bool applyAllOff() override;
    [[nodiscard]] bool updateZoneMetadata(int zoneIndex, const QString& name, int ledCount) override;
    [[nodiscard]] bool usesLocalFrameRendering() const override;
    [[nodiscard]] bool usesLocalFrameRenderingForEffect(int zoneIndex, const RgbEffect& effect) const override;
    [[nodiscard]] QString previewZoneEffectWrite(int zoneIndex, const RgbEffect& effect) const override;
    [[nodiscard]] QString lastHardwareWriteStatus() const override;
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability capability) const override;
    [[nodiscard]] bool supportsEffect(int effectType) const override;
    [[nodiscard]] bool supportsEffectSpeed(int effectType) const override;
    [[nodiscard]] bool supportsEffectBrightness(int effectType) const override;
    [[nodiscard]] bool supportsZoneEffect(int zoneIndex, int effectType) const override;
    [[nodiscard]] bool supportsZoneEffectSpeed(int zoneIndex, int effectType) const override;
    [[nodiscard]] bool supportsZoneEffectBrightness(int zoneIndex, int effectType) const override;

private:
    void initializeZones();
    void applyLocalAllOff();
    [[nodiscard]] QString writeDisabledReason() const;
    [[nodiscard]] bool sendApprovedPacket(
        const hardware::linux::AsusAuraHidProtocolResult& protocol,
        const QString& operation
    );
    [[nodiscard]] bool supportsHostStreamedEffect(int zoneIndex, int effectType) const;
    [[nodiscard]] int fixedZoneCount() const;
    [[nodiscard]] bool isAddressableZone(int zoneIndex) const;

    asus_aura_platform::ProbeDevice m_device;
    asus_aura_platform::DiscoverySupportInfo m_support;
    bool m_configTableVerified {false};
    hardware::linux::AsusAuraConfigTable m_configTable;
    QString m_configSummary;
    QString m_lastHardwareWriteStatus;
    asus_aura_platform::HidDeviceWriter m_writer;
};

} // namespace lumacore
