// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbDevice.h"
#include "ipc/DaemonClient.h"

#include <QHash>
#include <QJsonObject>

#include <functional>
#include <memory>

namespace lumacore {

class DaemonRgbDevice final : public RgbDevice
{
    Q_OBJECT

public:
    using OperationHandler = std::function<void(bool success, QString error)>;

    struct EffectSupport {
        bool supported {false};
        bool speed {false};
        bool brightness {false};
    };

    DaemonRgbDevice(QJsonObject snapshot, std::shared_ptr<DaemonClient> client, QObject* parent = nullptr);

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override;
    [[nodiscard]] QString discoveryIdentity() const override;
    [[nodiscard]] QString discoverySupportStage() const override;
    [[nodiscard]] QString discoverySupportStatus() const override;
    [[nodiscard]] QString discoverySupportFamily() const override;
    [[nodiscard]] QString discoverySupportNotes() const override;
    [[nodiscard]] bool discoveryCataloged() const override;
    [[nodiscard]] bool discoveryWriteCapableBackend() const override;
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
    [[nodiscard]] bool writeConfirmed() const;
    [[nodiscard]] bool writeRequiresConfirmation() const;
    [[nodiscard]] bool supportsEffect(int effectType) const override;
    [[nodiscard]] bool supportsEffectSpeed(int effectType) const override;
    [[nodiscard]] bool supportsEffectBrightness(int effectType) const override;
    [[nodiscard]] bool supportsZoneEffect(int zoneIndex, int effectType) const override;
    [[nodiscard]] bool supportsZoneEffectSpeed(int zoneIndex, int effectType) const override;
    [[nodiscard]] bool supportsZoneEffectBrightness(int zoneIndex, int effectType) const override;
    void setWriteConfirmed(bool confirmed);
    [[nodiscard]] quint64 applyZoneEffectAsync(
        int zoneIndex,
        const RgbEffect& effect,
        bool dryRunExpected,
        OperationHandler handler
    );
    [[nodiscard]] quint64 applyAllOffAsync(bool dryRunExpected, OperationHandler handler);
    [[nodiscard]] quint64 updateZoneMetadataAsync(
        int zoneIndex,
        const QString& name,
        int ledCount,
        OperationHandler handler
    );

private:
    struct FrameStreamState {
        quint64 inFlightRequestId {0};
        QVector<RgbColor> pendingColors;
        bool hasPendingFrame {false};
        int suspensionCount {0};
    };

    void applyLocalZoneEffect(int zoneIndex, const RgbEffect& effect);
    void applyLocalAllOff();
    [[nodiscard]] bool sendZoneFrame(int zoneIndex, const QVector<RgbColor>& colors);
    void suspendZoneFrames(int zoneIndex);
    void resumeZoneFrames(int zoneIndex);
    void suspendAllZoneFrames();
    void resumeAllZoneFrames();
    [[nodiscard]] bool supportsHostStreamedEffect(int zoneIndex, int effectType) const;

    int m_daemonDeviceIndex {-1};
    QString m_discoveryIdentity;
    QString m_discoverySupportStage;
    QString m_discoverySupportStatus;
    QString m_discoverySupportFamily;
    QString m_discoverySupportNotes;
    bool m_discoveryCataloged {false};
    bool m_discoveryWriteCapableBackend {false};
    BackendCapabilities m_capabilities {BackendCapability::None};
    PermissionResult m_permission {PermissionStatus::Denied, QStringLiteral("Daemon permission snapshot unavailable.")};
    QHash<QString, PermissionResult> m_permissions;
    QHash<int, EffectSupport> m_effectSupport;
    QVector<QHash<int, EffectSupport>> m_zoneEffectSupport;
    QHash<int, FrameStreamState> m_frameStreamStates;
    bool m_writeConfirmed {false};
    QString m_lastHardwareWriteStatus;
    std::shared_ptr<DaemonClient> m_client;
};

} // namespace lumacore
