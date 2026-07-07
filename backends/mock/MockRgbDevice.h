// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbDevice.h"

namespace lumacore {

struct MockRgbDeviceConfig {
    QString id;
    QString name;
    QString vendor;
    RgbDeviceType type {RgbDeviceType::Unknown};
    QVector<RgbZone> zones;
    BackendCapabilities capabilities {BackendCapability::None};
    PermissionResult runtimeWritePermission {PermissionStatus::Denied, {}};
    bool failWrites {false};
    bool likelyRgbController {false};
};

class MockRgbDevice final : public RgbDevice
{
    Q_OBJECT

public:
    explicit MockRgbDevice(QObject* parent = nullptr);
    explicit MockRgbDevice(MockRgbDeviceConfig config, QObject* parent = nullptr);

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override;
    [[nodiscard]] bool applyZoneEffect(int zoneIndex, const RgbEffect& effect) override;
    [[nodiscard]] bool applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors) override;
    [[nodiscard]] bool applyAllOff() override;
    [[nodiscard]] QString lastHardwareWriteStatus() const override;
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability capability) const override;

private:
    [[nodiscard]] bool rejectWrite(const QString& operation);

    BackendCapabilities m_capabilities {BackendCapability::None};
    PermissionResult m_runtimeWritePermission {PermissionStatus::Denied, {}};
    bool m_failWrites {false};
    QString m_lastHardwareWriteStatus;
};

} // namespace lumacore
