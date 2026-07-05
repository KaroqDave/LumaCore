// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/DeviceManager.h"
#include "ipc/DaemonClient.h"

#include <QColor>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <functional>
#include <memory>

namespace lumacore {

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(QString profilesDirectory READ profilesDirectory CONSTANT)
    Q_PROPERTY(QString backendDisplayName READ backendDisplayName NOTIFY backendInfoChanged)
    Q_PROPERTY(QString backendId READ backendId NOTIFY backendInfoChanged)
    Q_PROPERTY(QString backendEffectiveId READ backendEffectiveId NOTIFY backendInfoChanged)
    Q_PROPERTY(QString backendDescription READ backendDescription NOTIFY backendInfoChanged)
    Q_PROPERTY(QString backendCapabilitiesText READ backendCapabilitiesText NOTIFY backendInfoChanged)
    Q_PROPERTY(int backendDeviceCount READ backendDeviceCount NOTIFY backendInfoChanged)
    Q_PROPERTY(QString setupStatusLevel READ setupStatusLevel NOTIFY setupStatusChanged)
    Q_PROPERTY(QString setupStatusSummary READ setupStatusSummary NOTIFY setupStatusChanged)
    Q_PROPERTY(QString setupStatusDetail READ setupStatusDetail NOTIFY setupStatusChanged)
    Q_PROPERTY(QString setupStatusAction READ setupStatusAction NOTIFY setupStatusChanged)
    Q_PROPERTY(bool setupAttentionRequired READ setupAttentionRequired NOTIFY setupStatusChanged)
    Q_PROPERTY(bool daemonConnected READ daemonConnected NOTIFY daemonInfoChanged)
    Q_PROPERTY(QString daemonState READ daemonState NOTIFY daemonInfoChanged)
    Q_PROPERTY(QString daemonSocketPath READ daemonSocketPath NOTIFY daemonInfoChanged)
    Q_PROPERTY(QString daemonVersion READ daemonVersion NOTIFY daemonInfoChanged)
    Q_PROPERTY(QString daemonLastError READ daemonLastError NOTIFY daemonInfoChanged)
    Q_PROPERTY(bool daemonRecoveryBusy READ daemonRecoveryBusy NOTIFY daemonInfoChanged)
    Q_PROPERTY(bool daemonDryRunKnown READ daemonDryRunKnown NOTIFY daemonInfoChanged)
    Q_PROPERTY(bool daemonDryRunEnabled READ daemonDryRunEnabled NOTIFY daemonInfoChanged)
    Q_PROPERTY(bool daemonScheduleSupported READ daemonScheduleSupported NOTIFY daemonInfoChanged)
    Q_PROPERTY(bool daemonDryRunMismatch READ daemonDryRunMismatch NOTIFY daemonDryRunSyncChanged)
    Q_PROPERTY(bool dryRunEnabled READ dryRunEnabled WRITE setDryRunEnabled NOTIFY dryRunEnabledChanged)
    Q_PROPERTY(int pendingDaemonOperations READ pendingDaemonOperations NOTIFY pendingDaemonOperationsChanged)
    Q_PROPERTY(bool profileApplyInProgress READ profileApplyInProgress NOTIFY profileApplyInProgressChanged)
    Q_PROPERTY(QStringList deviceGroupNames READ deviceGroupNames NOTIFY deviceGroupsChanged)

public:
    explicit AppController(
        DeviceManager* deviceManager,
        std::shared_ptr<DaemonClient> daemonClient = {},
        QObject* parent = nullptr
    );

    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString logText() const;
    [[nodiscard]] QString profilesDirectory() const;
    [[nodiscard]] QString backendDisplayName() const;
    [[nodiscard]] QString backendId() const;
    [[nodiscard]] QString backendEffectiveId() const;
    [[nodiscard]] QString backendDescription() const;
    [[nodiscard]] QString backendCapabilitiesText() const;
    [[nodiscard]] int backendDeviceCount() const;
    [[nodiscard]] QString setupStatusLevel() const;
    [[nodiscard]] QString setupStatusSummary() const;
    [[nodiscard]] QString setupStatusDetail() const;
    [[nodiscard]] QString setupStatusAction() const;
    [[nodiscard]] bool setupAttentionRequired() const;
    [[nodiscard]] bool daemonConnected() const;
    [[nodiscard]] QString daemonState() const;
    [[nodiscard]] QString daemonSocketPath() const;
    [[nodiscard]] QString daemonVersion() const;
    [[nodiscard]] QString daemonLastError() const;
    [[nodiscard]] bool daemonRecoveryBusy() const;
    [[nodiscard]] bool daemonDryRunKnown() const;
    [[nodiscard]] bool daemonDryRunEnabled() const;
    [[nodiscard]] bool daemonScheduleSupported() const;
    [[nodiscard]] bool daemonDryRunMismatch() const;
    [[nodiscard]] bool dryRunEnabled() const;
    [[nodiscard]] int pendingDaemonOperations() const;
    [[nodiscard]] bool profileApplyInProgress() const;
    [[nodiscard]] QStringList deviceGroupNames() const;
    void setDryRunEnabled(bool enabled);

    Q_INVOKABLE bool applyEffect(int deviceIndex, int zoneIndex, int effectType, const QColor& color, double speed, int brightness);
    Q_INVOKABLE int zoneEffectType(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE QColor zoneEffectColor(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE double zoneEffectSpeed(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE int zoneEffectBrightness(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE bool zoneEffectsPanelEnabled(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE void setZoneEffectsPanelEnabled(int deviceIndex, int zoneIndex, bool enabled);
    Q_INVOKABLE bool updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount);
    Q_INVOKABLE int zoneCount(int deviceIndex) const;
    Q_INVOKABLE bool deviceWritable(int deviceIndex) const;
    Q_INVOKABLE QString devicePermissionReason(int deviceIndex) const;
    Q_INVOKABLE QString deviceLastHardwareWriteStatus(int deviceIndex) const;
    Q_INVOKABLE bool deviceSupportsEffect(int deviceIndex, int effectType) const;
    Q_INVOKABLE bool deviceSupportsEffectSpeed(int deviceIndex, int effectType) const;
    Q_INVOKABLE bool deviceSupportsEffectBrightness(int deviceIndex, int effectType) const;
    Q_INVOKABLE bool zoneSupportsEffect(int deviceIndex, int zoneIndex, int effectType) const;
    Q_INVOKABLE bool zoneSupportsEffectSpeed(int deviceIndex, int zoneIndex, int effectType) const;
    Q_INVOKABLE bool zoneSupportsEffectBrightness(int deviceIndex, int zoneIndex, int effectType) const;
    Q_INVOKABLE bool globalTargetSupportsEffect(const QString& groupName, int effectType) const;
    Q_INVOKABLE bool globalTargetSupportsEffectSpeed(const QString& groupName, int effectType) const;
    Q_INVOKABLE bool globalTargetSupportsEffectBrightness(const QString& groupName, int effectType) const;
    Q_INVOKABLE bool deviceRequiresConfirmation(int deviceIndex) const;
    Q_INVOKABLE bool deviceWriteConfirmed(int deviceIndex) const;
    Q_INVOKABLE bool confirmDeviceWrites(int deviceIndex);
    Q_INVOKABLE bool revokeDeviceWrites(int deviceIndex);
    Q_INVOKABLE bool allOffDevice(int deviceIndex);
    Q_INVOKABLE bool applyEffectGlobally(int effectType, const QColor& color, double speed, int brightness);
    Q_INVOKABLE bool applyEffectToDeviceGroup(const QString& groupName, int effectType, const QColor& color, double speed, int brightness);
    Q_INVOKABLE bool setZoneBrightness(int deviceIndex, int zoneIndex, int brightness);
    Q_INVOKABLE bool setGlobalBrightness(int brightness);
    Q_INVOKABLE bool setDeviceGroupBrightness(const QString& groupName, int brightness);
    Q_INVOKABLE bool allOffAllDevices();
    Q_INVOKABLE bool allOffDeviceGroup(const QString& groupName);
    Q_INVOKABLE QVariantList deviceGroupInfos() const;
    Q_INVOKABLE QVariantList deviceGroupDeviceOptions() const;
    Q_INVOKABLE QStringList deviceGroupDeviceIds(const QString& groupName) const;
    Q_INVOKABLE bool saveDeviceGroup(const QString& groupName, const QStringList& deviceIds);
    Q_INVOKABLE bool deleteDeviceGroup(const QString& groupName);
    Q_INVOKABLE bool markDeviceRgbController(int deviceIndex);
    Q_INVOKABLE bool removeDeviceRgbController(int deviceIndex);
    Q_INVOKABLE bool resetDeviceRgbControllerOverride(int deviceIndex);
    Q_INVOKABLE QString deviceId(int deviceIndex) const;
    Q_INVOKABLE QString deviceName(int deviceIndex) const;
    Q_INVOKABLE int deviceIndexForId(const QString& deviceId) const;
    Q_INVOKABLE int zoneIndexForName(int deviceIndex, const QString& zoneName) const;
    Q_INVOKABLE QString zoneName(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE int zoneLedCount(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE QString zoneColorHex(int deviceIndex, int zoneIndex) const;
    Q_INVOKABLE bool saveProfile(const QString& profileName);
    Q_INVOKABLE bool loadProfile(const QString& profileName);
    Q_INVOKABLE QVariantMap applyProfileWithReport(const QString& profileName);
    Q_INVOKABLE bool applyProfileAsync(const QString& profileName);
    Q_INVOKABLE bool deleteProfile(const QString& profileName);
    Q_INVOKABLE bool renameProfile(const QString& oldProfileName, const QString& newProfileName);
    Q_INVOKABLE QString importProfile(const QUrl& sourceUrl);
    Q_INVOKABLE bool exportProfile(const QString& profileName, const QUrl& destinationUrl);
    Q_INVOKABLE QVariantMap diagnosticsReport() const;
    Q_INVOKABLE QString diagnosticsSummaryText() const;
    Q_INVOKABLE bool copyDiagnosticsSummary();
    Q_INVOKABLE bool exportDiagnostics(const QUrl& destinationUrl);
    Q_INVOKABLE QVariantMap profileCompatibility(const QString& profileName);
    Q_INVOKABLE bool profileExists(const QString& profileName) const;
    Q_INVOKABLE QStringList profileNames() const;
    Q_INVOKABLE bool retryDaemonConnection();
    Q_INVOKABLE bool rescanDaemonDevices();
    [[nodiscard]] bool applyProfileOnLaunch(const QString& profileName);
    [[nodiscard]] bool applyScheduledProfile(const QString& profileName);
    void enableDaemonRecovery();

signals:
    void statusMessageChanged();
    void logTextChanged();
    void profilesChanged();
    void zoneDataChanged(int deviceIndex, int zoneIndex);
    void backendInfoChanged();
    void setupStatusChanged();
    void daemonInfoChanged();
    void daemonDryRunSyncChanged();
    void writeConfirmationChanged(int deviceIndex);
    void dryRunEnabledChanged();
    void deviceGroupsChanged();
    void pendingDaemonOperationsChanged();
    void profileApplyInProgressChanged();
    void daemonDevicesRefreshed();
    void globalOperationFinished(QVariantMap result);
    void profileApplyFinished(QVariantMap result);

private:
    struct CachedSetupStatus {
        bool valid {false};
        QString level;
        QString summary;
        QString detail;
        QString action;
    };

    [[nodiscard]] const CachedSetupStatus& cachedSetupStatus() const;
    [[nodiscard]] RgbDevice* deviceAt(int deviceIndex);
    [[nodiscard]] const RgbDevice* deviceAt(int deviceIndex) const;
    [[nodiscard]] const RgbZone* zoneAt(int deviceIndex, int zoneIndex) const;
    void appendLog(const QString& message);
    void setStatusMessage(const QString& message);
    void setLocalDaemonWriteConfirmed(int deviceIndex, bool confirmed);
    void refreshBackendInfo();
    void refreshDaemonActivityLog();
    void syncDaemonDryRun();
    void beginDaemonOperation();
    void endDaemonOperation();
    // Starts host frame streaming for an applied animated effect that the device
    // renders on the host, or stops any streaming for that zone otherwise. Shared
    // by the single-zone, global, and profile apply paths so all three animate
    // consistently.
    void syncZoneFrameStreaming(int deviceIndex, int zoneIndex, const RgbEffect& effect);
    bool applyGlobalEffectInternal(
        int effectType,
        const QColor& color,
        double speed,
        int brightness,
        bool preserveCurrentEffect,
        const QStringList& targetDeviceIds = {},
        const QString& targetName = {}
    );
    bool allOffDevicesInternal(const QStringList& targetDeviceIds = {}, const QString& targetName = {});
    bool applyProfileInternal(
        const QString& profileName,
        std::function<void(const QVariantMap&)> completion,
        bool emitFinished
    );
    [[nodiscard]] static QString normalizeDeviceGroupName(const QString& groupName);
    [[nodiscard]] QStringList storedDeviceGroupIds(const QString& groupName) const;
    bool refreshDaemonDevices(bool recoveredConnection);

    DeviceManager* m_deviceManager {nullptr};
    std::shared_ptr<DaemonClient> m_daemonClient;
    QString m_statusMessage;
    QStringList m_logLines;
    int m_pendingDaemonOperations {0};
    bool m_daemonRecoveryEnabled {false};
    bool m_daemonDevicesLoaded {false};
    QString m_pendingScheduledProfile;
    bool m_daemonRefreshInProgress {false};
    bool m_profileApplyInProgress {false};
    bool m_daemonDryRunSyncInProgress {false};
    bool m_pendingDaemonDryRunSync {false};
    mutable CachedSetupStatus m_setupStatus;
};

} // namespace lumacore
