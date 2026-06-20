#pragma once

#include "core/RgbDevice.h"
#include "ipc/DaemonClient.h"

#include <QHash>
#include <QJsonObject>

#include <memory>

namespace lumacore {

class DaemonRgbDevice final : public RgbDevice
{
    Q_OBJECT

public:
    struct EffectSupport {
        bool supported {false};
        bool speed {false};
        bool brightness {false};
    };

    DaemonRgbDevice(QJsonObject snapshot, std::shared_ptr<DaemonClient> client, QObject* parent = nullptr);

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
    [[nodiscard]] bool writeConfirmed() const;
    [[nodiscard]] bool writeRequiresConfirmation() const;
    [[nodiscard]] bool supportsEffect(int effectType) const override;
    [[nodiscard]] bool supportsEffectSpeed(int effectType) const override;
    [[nodiscard]] bool supportsEffectBrightness(int effectType) const override;
    void setWriteConfirmed(bool confirmed);

private:
    int m_daemonDeviceIndex {-1};
    BackendCapabilities m_capabilities {BackendCapability::None};
    PermissionResult m_permission {PermissionStatus::Denied, QStringLiteral("Daemon permission snapshot unavailable.")};
    QHash<QString, PermissionResult> m_permissions;
    QHash<int, EffectSupport> m_effectSupport;
    bool m_writeConfirmed {false};
    QString m_lastHardwareWriteStatus;
    std::shared_ptr<DaemonClient> m_client;
};

} // namespace lumacore
