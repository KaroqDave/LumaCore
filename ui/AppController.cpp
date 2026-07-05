// SPDX-License-Identifier: GPL-2.0-or-later

#include "ui/AppController.h"

#include "backends/daemon/DaemonBackend.h"
#include "backends/daemon/DaemonRgbDevice.h"
#include "core/ActivityLog.h"
#include "core/PermissionGate.h"
#include "core/PortablePaths.h"
#include "core/ProfilePlan.h"
#include "core/ProfileStore.h"
#include "core/RgbBackend.h"
#include "core/RgbColor.h"
#include "core/RgbEffect.h"
#include "ui/DeviceGroupStore.h"
#include "ui/ZoneEffectPreferenceStore.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QSysInfo>
#include <QTimer>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <utility>

namespace lumacore {

namespace {

bool isValidEffectType(int effectType)
{
    return isValidRgbEffectType(effectType);
}

bool isAnimatedEffect(int effectType)
{
    return isAnimatedRgbEffectType(effectType);
}

QString sanitizedDiagnosticString(QString value, const QString& profilesDirectory = {})
{
    const QString homePath = QDir::toNativeSeparators(QDir::homePath());
    const QString tempPath = QDir::toNativeSeparators(QDir::tempPath());
    const QString appDataPath = QDir::toNativeSeparators(portableDataRoot());
    const QString normalizedProfilesDirectory = QDir::toNativeSeparators(profilesDirectory);

    const auto replacePath = [&value](const QString& path, const QString& replacement) {
        if (!path.isEmpty()) {
            value.replace(path, replacement, Qt::CaseInsensitive);
            value.replace(QDir::fromNativeSeparators(path), replacement, Qt::CaseInsensitive);
        }
    };

    replacePath(normalizedProfilesDirectory, QStringLiteral("<profiles>"));
    replacePath(appDataPath, QStringLiteral("<app-data>"));
    replacePath(tempPath, QStringLiteral("<temp>"));
    replacePath(homePath, QStringLiteral("<home>"));
    return value;
}

QString permissionStatusDiagnosticName(PermissionStatus status)
{
    switch (status) {
    case PermissionStatus::Granted:
        return QStringLiteral("granted");
    case PermissionStatus::Denied:
        return QStringLiteral("denied");
    case PermissionStatus::RequiresConfirmation:
        return QStringLiteral("requires-confirmation");
    case PermissionStatus::NotApplicable:
        return QStringLiteral("not-applicable");
    }
    return QStringLiteral("unknown");
}

QVariantMap permissionToDiagnostics(const PermissionResult& permission, const std::function<QString(const QString&)>& sanitize)
{
    return QVariantMap {
        {QStringLiteral("status"), permissionStatusDiagnosticName(permission.status)},
        {QStringLiteral("reason"), sanitize(permission.reason)},
    };
}

QVariantMap effectToDiagnostics(const RgbEffect& effect)
{
    return QVariantMap {
        {QStringLiteral("type"), rgbEffectTypeToString(effect.type())},
        {QStringLiteral("color"), effect.color().toHexString()},
        {QStringLiteral("speed"), effect.speed()},
        {QStringLiteral("brightness"), effect.brightness()},
    };
}

struct GlobalOperationState {
    QString operation;
    QString targetName;
    QString targetKind;
    int total {0};
    int applied {0};
    int skipped {0};
    int failed {0};
    int pending {0};
    bool dispatching {true};
    QStringList details;
};

QStringList uniqueNonEmptyStrings(const QStringList& values)
{
    QStringList result;
    for (const QString& value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty() && !result.contains(normalized)) {
            result.append(normalized);
        }
    }
    return result;
}

QString bundledDaemonExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("lumacore-daemon.exe");
#else
    return QStringLiteral("lumacore-daemon");
#endif
}

struct SetupStatus {
    QString level;
    QString summary;
    QString detail;
    QString action;
};

QString diagnosticsSummaryTextFor(const QVariantMap& report)
{
    const QVariantMap application = report.value(QStringLiteral("application")).toMap();
    const QVariantMap backend = report.value(QStringLiteral("backend")).toMap();
    const QVariantMap setup = report.value(QStringLiteral("setup")).toMap();
    const QVariantMap daemon = report.value(QStringLiteral("daemon")).toMap();
    const QVariantMap safety = report.value(QStringLiteral("safety")).toMap();
    const QVariantMap counts = report.value(QStringLiteral("counts")).toMap();

    QStringList lines {
        QStringLiteral("LumaCore diagnostics summary"),
        QStringLiteral("Generated: %1").arg(report.value(QStringLiteral("generatedAt")).toString()),
        QStringLiteral("Application: %1 %2 on %3")
            .arg(
                application.value(QStringLiteral("name")).toString(),
                application.value(QStringLiteral("version")).toString(),
                application.value(QStringLiteral("platform")).toString()
            ),
        QStringLiteral("Backend: %1 (%2), %3 device(s)")
            .arg(
                backend.value(QStringLiteral("displayName")).toString(),
                backend.value(QStringLiteral("id")).toString()
            )
            .arg(counts.value(QStringLiteral("devices")).toInt()),
        QStringLiteral("Setup: %1 - %2")
            .arg(
                setup.value(QStringLiteral("summary")).toString(),
                setup.value(QStringLiteral("detail")).toString()
            ),
        QStringLiteral("Daemon: %1, connected=%2, version=%3")
            .arg(
                daemon.value(QStringLiteral("state")).toString(),
                daemon.value(QStringLiteral("connected")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"),
                daemon.value(QStringLiteral("version")).toString().isEmpty()
                    ? QStringLiteral("unavailable")
                    : daemon.value(QStringLiteral("version")).toString()
            ),
        QStringLiteral("Safety: dry-run=%1, daemon dry-run=%2, writable=%3, confirmation-required=%4, read-only=%5")
            .arg(safety.value(QStringLiteral("dryRunEnabled")).toBool() ? QStringLiteral("enabled") : QStringLiteral("disabled"))
            .arg(
                safety.value(QStringLiteral("daemonDryRunKnown")).toBool()
                    ? (safety.value(QStringLiteral("daemonDryRunEnabled")).toBool() ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                    : QStringLiteral("unknown")
            )
            .arg(counts.value(QStringLiteral("writableDevices")).toInt())
            .arg(counts.value(QStringLiteral("confirmationRequiredDevices")).toInt())
            .arg(counts.value(QStringLiteral("readOnlyDevices")).toInt()),
        QStringLiteral("Profiles: %1 name(s); profile contents are not included")
            .arg(counts.value(QStringLiteral("profiles")).toInt()),
        QStringLiteral("Activity: %1 recent line(s)")
            .arg(counts.value(QStringLiteral("activityLines")).toInt()),
    };

    const QString action = setup.value(QStringLiteral("action")).toString();
    if (!action.isEmpty()) {
        lines.append(QStringLiteral("Suggested action: %1").arg(action));
    }

    return lines.join(QLatin1Char('\n'));
}

struct ProfileApplyState {
    ProfileApplySummary summary;
    int pending {0};
    bool dispatching {true};
};

SetupStatus setupStatusFor(const AppController& controller)
{
    const bool daemonBacked = !controller.daemonSocketPath().isEmpty();
    if (daemonBacked) {
        const QString daemonState = controller.daemonState();
        if (daemonState == QStringLiteral("Refreshing devices")) {
            return {
                QStringLiteral("info"),
                QStringLiteral("Refreshing devices"),
                QStringLiteral("LumaCore is reloading the device inventory from the daemon."),
                QStringLiteral("Wait for the refresh to finish before changing effects."),
            };
        }
        if (daemonState == QStringLiteral("Connecting") || daemonState == QStringLiteral("Reconnecting")) {
            return {
                QStringLiteral("warning"),
                daemonState == QStringLiteral("Connecting")
                    ? QStringLiteral("Connecting to daemon")
                    : QStringLiteral("Reconnecting to daemon"),
                QStringLiteral("The GUI cannot refresh devices or send RGB commands until the daemon connection is ready."),
                QStringLiteral("Start lumacore-daemon if it is not running, then use Retry if needed."),
            };
        }
        if (daemonState == QStringLiteral("Incompatible daemon")) {
            return {
                QStringLiteral("error"),
                QStringLiteral("Daemon version mismatch"),
                QStringLiteral("The GUI reached a daemon, but its protocol version is not compatible with this build."),
                QStringLiteral("Restart the matching lumacore-daemon build."),
            };
        }
        if (!controller.daemonConnected()) {
            const QString lastError = controller.daemonLastError();
            return {
                QStringLiteral("warning"),
                QStringLiteral("Daemon offline"),
                lastError.isEmpty()
                    ? QStringLiteral("The GUI is running, but no compatible LumaCore daemon is connected.")
                    : QStringLiteral("The GUI is running, but the daemon connection failed: %1").arg(lastError),
                QStringLiteral("Start lumacore-daemon, then use Retry."),
            };
        }
    }

    const int deviceCount = controller.backendDeviceCount();
    if (daemonBacked
        && controller.daemonConnected()
        && controller.daemonDryRunMismatch()) {
        return {
            QStringLiteral("warning"),
            QStringLiteral("Daemon dry-run mismatch"),
            QStringLiteral("The GUI dry-run setting is %1, but the daemon reports dry-run %2.")
                .arg(
                    controller.dryRunEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                    controller.daemonDryRunEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled")
                ),
            QStringLiteral("Toggle dry-run or rescan to synchronize the daemon before applying hardware changes."),
        };
    }

    if (deviceCount == 0) {
        return {
            QStringLiteral("warning"),
            QStringLiteral("No devices loaded"),
            QStringLiteral("The active backend is available, but it has not reported any devices."),
            daemonBacked
                ? QStringLiteral("Use Rescan or check daemon permissions and backend selection.")
                : QStringLiteral("Check backend selection and local permissions."),
        };
    }

    int writableDevices = 0;
    int confirmationRequiredDevices = 0;
    for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
        if (!controller.deviceWritable(deviceIndex)) {
            continue;
        }
        ++writableDevices;
        if (controller.deviceRequiresConfirmation(deviceIndex)
            && !controller.deviceWriteConfirmed(deviceIndex)) {
            ++confirmationRequiredDevices;
        }
    }

    if (writableDevices == 0) {
        QString readOnlyDetail = QStringLiteral("Devices are visible, but none of the loaded devices are verified for RGB writes.");
        for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
            const QString reason = controller.devicePermissionReason(deviceIndex).trimmed();
            if (reason.isEmpty()) {
                continue;
            }

            readOnlyDetail = QStringLiteral("%1 %2: %3")
                                 .arg(readOnlyDetail, controller.deviceName(deviceIndex), reason);
            break;
        }
        return {
            QStringLiteral("info"),
            QStringLiteral("Read-only inventory"),
            readOnlyDetail,
            QStringLiteral("Use diagnostics to check backend selection, device identity, and write-gate verification."),
        };
    }

    if (controller.dryRunEnabled()) {
        return {
            QStringLiteral("warning"),
            QStringLiteral("Dry-run active"),
            QStringLiteral("RGB writes are being logged instead of applied to hardware."),
            QStringLiteral("Disable dry-run when you are ready to apply real lighting changes."),
        };
    }

    if (confirmationRequiredDevices > 0) {
        return {
            QStringLiteral("warning"),
            QStringLiteral("Hardware confirmation required"),
            QStringLiteral("%1 write-capable device(s) require confirmation before real hardware writes.")
                .arg(confirmationRequiredDevices),
            QStringLiteral("Select each device and confirm writes for this daemon session."),
        };
    }

    return {
        QStringLiteral("ready"),
        QStringLiteral("Ready"),
        QStringLiteral("%1 device(s) loaded. The active backend can apply supported RGB changes.")
            .arg(deviceCount),
        QStringLiteral("Save a profile once the current lighting state is correct."),
    };
}

} // namespace

AppController::AppController(DeviceManager* deviceManager, std::shared_ptr<DaemonClient> daemonClient, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
    , m_daemonClient(std::move(daemonClient))
{
    setStatusMessage(QStringLiteral("Connecting to LumaCore daemon."));

    // Connected first so the cache is invalidated before any later-connected
    // consumer (QML bindings, diagnostics) re-reads the setup status.
    connect(this, &AppController::setupStatusChanged, this, [this] {
        m_setupStatus.valid = false;
    });

    // daemonDryRunMismatch depends on both the daemon-reported state and the
    // GUI setting, so re-notify it whenever either input changes.
    connect(this, &AppController::daemonInfoChanged, this, &AppController::daemonDryRunSyncChanged);
    connect(this, &AppController::dryRunEnabledChanged, this, &AppController::daemonDryRunSyncChanged);

    if (m_deviceManager == nullptr) {
        return;
    }

    m_logLines = m_deviceManager->activityLog().formattedLines();

    connect(m_deviceManager, &DeviceManager::logMessage, this, [this](const QString& message) {
        appendLog(message);
    });

    connect(&m_deviceManager->activityLog(), &ActivityLog::entryAdded, this, [this](const LogEntry& entry) {
        setStatusMessage(entry.message);
    });

    connect(m_deviceManager, &DeviceManager::zoneChanged, this, [this](int deviceIndex, int zoneIndex) {
        emit zoneDataChanged(deviceIndex, zoneIndex);
    });

    connect(m_deviceManager, &DeviceManager::devicesChanged, this, [this]() {
        refreshBackendInfo();
        emit deviceGroupsChanged();
        // The setup status is derived from the device set (count + writability),
        // so any device-set change must invalidate the cached status.
        emit setupStatusChanged();
    });

    connect(m_deviceManager, &DeviceManager::dryRunEnabledChanged, this, [this]() {
        emit dryRunEnabledChanged();
        emit setupStatusChanged();
    });

    if (m_daemonClient != nullptr) {
        connect(m_daemonClient.get(), &DaemonClient::connectionStateChanged, this, [this]() {
            emit daemonInfoChanged();
            emit setupStatusChanged();
            if (m_daemonClient->isConnected()) {
                syncDaemonDryRun();
                if (m_daemonRecoveryEnabled && !m_daemonDevicesLoaded && !m_daemonRefreshInProgress) {
                    QTimer::singleShot(0, this, [this] { refreshDaemonDevices(false); });
                }
            } else {
                m_daemonDevicesLoaded = false;
                if (m_deviceManager != nullptr) {
                    m_deviceManager->stopAllFrameStreaming();
                }
            }
            setStatusMessage(daemonConnected() ? QStringLiteral("Connected to LumaCore daemon.") : QStringLiteral("Daemon disconnected."));
        });
        connect(m_daemonClient.get(), &DaemonClient::lastErrorChanged, this, [this]() {
            emit daemonInfoChanged();
            emit setupStatusChanged();
            if (!daemonLastError().isEmpty()) {
                setStatusMessage(daemonLastError());
            }
        });
        connect(m_daemonClient.get(), &DaemonClient::daemonInfoChanged, this, [this]() {
            emit daemonInfoChanged();
            emit setupStatusChanged();
        });
        connect(m_daemonClient.get(), &DaemonClient::reconnected, this, [this]() {
            if (m_daemonRecoveryEnabled) {
                QTimer::singleShot(0, this, [this] { refreshDaemonDevices(true); });
            }
        });
        connect(m_daemonClient.get(), &DaemonClient::reconnectScheduled, this, [this](int attempt, int delayMs) {
            emit daemonInfoChanged();
            emit setupStatusChanged();
            setStatusMessage(
                delayMs > 0
                    ? QStringLiteral("Daemon offline. Reconnect attempt %1 in %2 ms.").arg(attempt).arg(delayMs)
                    : QStringLiteral("Retrying daemon connection.")
            );
        });
    }

    m_deviceManager->activityLog().info(
        LogCategory::System,
        QStringLiteral("LumaCore v%1 started (daemon client, Qt %2).")
            .arg(QStringLiteral(LUMACORE_VERSION), QString::fromUtf8(qVersion()))
    );

    refreshBackendInfo();
    refreshDaemonActivityLog();
}

QString AppController::statusMessage() const
{
    return m_statusMessage;
}

QString AppController::logText() const
{
    return m_logLines.join(QLatin1Char('\n'));
}

QString AppController::profilesDirectory() const
{
    return m_deviceManager == nullptr ? QString() : m_deviceManager->profilesDirectoryPath();
}

QString AppController::backendDisplayName() const
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    return m_deviceManager->backendRegistry().activeDescriptor().displayName;
}

QString AppController::backendId() const
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    return m_deviceManager->backendRegistry().activeDescriptor().id;
}

QString AppController::backendEffectiveId() const
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    const RgbBackend* backend = m_deviceManager->backendRegistry().activeBackend();
    if (const auto* daemonBackend = dynamic_cast<const DaemonBackend*>(backend)) {
        return daemonBackend->effectiveDescriptor().id;
    }

    return m_deviceManager->backendRegistry().activeDescriptor().id;
}

QString AppController::backendDescription() const
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    return m_deviceManager->backendRegistry().activeDescriptor().description;
}

QString AppController::backendCapabilitiesText() const
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    const BackendCapabilities capabilities = m_deviceManager->backendRegistry().activeDescriptor().capabilities;
    QStringList labels;
    if (capabilities.testFlag(BackendCapability::DiscoveryRead)) {
        labels.append(backendCapabilityDisplayName(BackendCapability::DiscoveryRead));
    }
    if (capabilities.testFlag(BackendCapability::ZoneColorWrite)) {
        labels.append(backendCapabilityDisplayName(BackendCapability::ZoneColorWrite));
    }
    if (capabilities.testFlag(BackendCapability::ZoneEffectWrite)) {
        labels.append(backendCapabilityDisplayName(BackendCapability::ZoneEffectWrite));
    }

    return labels.join(QStringLiteral(", "));
}

int AppController::backendDeviceCount() const
{
    return m_deviceManager == nullptr ? 0 : m_deviceManager->deviceCount();
}

const AppController::CachedSetupStatus& AppController::cachedSetupStatus() const
{
    if (!m_setupStatus.valid) {
        const SetupStatus status = setupStatusFor(*this);
        m_setupStatus.level = status.level;
        m_setupStatus.summary = status.summary;
        m_setupStatus.detail = status.detail;
        m_setupStatus.action = status.action;
        m_setupStatus.valid = true;
    }
    return m_setupStatus;
}

QString AppController::setupStatusLevel() const
{
    return cachedSetupStatus().level;
}

QString AppController::setupStatusSummary() const
{
    return cachedSetupStatus().summary;
}

QString AppController::setupStatusDetail() const
{
    return cachedSetupStatus().detail;
}

QString AppController::setupStatusAction() const
{
    return cachedSetupStatus().action;
}

bool AppController::setupAttentionRequired() const
{
    const QString level = setupStatusLevel();
    return level == QStringLiteral("warning") || level == QStringLiteral("error");
}

bool AppController::daemonConnected() const
{
    return m_daemonClient != nullptr
        && m_daemonClient->isConnected()
        && m_daemonClient->protocolCompatible();
}

QString AppController::daemonState() const
{
    if (m_daemonRefreshInProgress) {
        return QStringLiteral("Refreshing devices");
    }
    if (m_daemonClient == nullptr) {
        return QStringLiteral("Offline");
    }
    if (m_daemonClient->isConnecting()) {
        return m_daemonClient->reconnectAttempt() > 0
            ? QStringLiteral("Reconnecting")
            : QStringLiteral("Connecting");
    }
    if (!m_daemonClient->isConnected()) {
        return m_daemonClient->isReconnectScheduled()
            ? QStringLiteral("Reconnecting")
            : QStringLiteral("Offline");
    }
    if (!m_daemonClient->protocolCompatible()) {
        return QStringLiteral("Incompatible daemon");
    }
    return QStringLiteral("Connected");
}

QString AppController::daemonSocketPath() const
{
    return m_daemonClient == nullptr ? QString() : m_daemonClient->socketPath();
}

QString AppController::daemonVersion() const
{
    return m_daemonClient == nullptr ? QString() : m_daemonClient->daemonVersion();
}

QString AppController::daemonLastError() const
{
    return m_daemonClient == nullptr ? QString() : m_daemonClient->lastError();
}

bool AppController::daemonRecoveryBusy() const
{
    return m_daemonRefreshInProgress
        || (m_daemonClient != nullptr
            && (m_daemonClient->isConnecting() || m_daemonClient->isReconnectScheduled()));
}

bool AppController::daemonDryRunKnown() const
{
    return m_daemonClient != nullptr && m_daemonClient->daemonDryRunKnown();
}

bool AppController::daemonDryRunEnabled() const
{
    return m_daemonClient != nullptr && m_daemonClient->daemonDryRunEnabled();
}

bool AppController::daemonScheduleSupported() const
{
    return m_daemonClient != nullptr && m_daemonClient->daemonScheduleSupported();
}

bool AppController::daemonDryRunMismatch() const
{
    return daemonDryRunKnown() && daemonDryRunEnabled() != dryRunEnabled();
}

bool AppController::dryRunEnabled() const
{
    return m_deviceManager != nullptr && m_deviceManager->dryRunEnabled();
}

int AppController::pendingDaemonOperations() const
{
    return m_pendingDaemonOperations;
}

bool AppController::profileApplyInProgress() const
{
    return m_profileApplyInProgress;
}

QStringList AppController::deviceGroupNames() const
{
    return DeviceGroupStore().names();
}

QVariantList AppController::deviceGroupInfos() const
{
    QVariantList groups;
    if (m_deviceManager == nullptr) {
        return groups;
    }

    for (const QString& groupName : deviceGroupNames()) {
        const QStringList deviceIds = storedDeviceGroupIds(groupName);
        int presentDevices = 0;
        int writableDevices = 0;
        int zoneCount = 0;
        for (const QString& deviceId : deviceIds) {
            const int deviceIndex = deviceIndexForId(deviceId);
            const RgbDevice* device = deviceAt(deviceIndex);
            if (device == nullptr) {
                continue;
            }
            ++presentDevices;
            zoneCount += device->zones().size();
            if (deviceWritable(deviceIndex)) {
                ++writableDevices;
            }
        }

        groups.append(QVariantMap {
            {QStringLiteral("name"), groupName},
            {QStringLiteral("deviceIds"), deviceIds},
            {QStringLiteral("deviceCount"), deviceIds.size()},
            {QStringLiteral("presentDeviceCount"), presentDevices},
            {QStringLiteral("writableDeviceCount"), writableDevices},
            {QStringLiteral("zoneCount"), zoneCount},
            {
                QStringLiteral("summary"),
                QStringLiteral("%1 present, %2 writable, %3 zone(s)")
                    .arg(presentDevices)
                    .arg(writableDevices)
                    .arg(zoneCount),
            },
        });
    }

    return groups;
}

QVariantList AppController::deviceGroupDeviceOptions() const
{
    QVariantList devices;
    if (m_deviceManager == nullptr) {
        return devices;
    }

    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = deviceAt(deviceIndex);
        if (device == nullptr) {
            continue;
        }

        devices.append(QVariantMap {
            {QStringLiteral("index"), deviceIndex},
            {QStringLiteral("id"), device->id()},
            {QStringLiteral("name"), device->name()},
            {QStringLiteral("vendor"), device->vendor()},
            {QStringLiteral("zoneCount"), device->zones().size()},
            {QStringLiteral("writable"), deviceWritable(deviceIndex)},
            {QStringLiteral("isRgbController"), device->isRgbController()},
        });
    }

    return devices;
}

QStringList AppController::deviceGroupDeviceIds(const QString& groupName) const
{
    return storedDeviceGroupIds(groupName);
}

bool AppController::saveDeviceGroup(const QString& groupName, const QStringList& deviceIds)
{
    const QString normalizedName = normalizeDeviceGroupName(groupName);
    const QStringList normalizedIds = uniqueNonEmptyStrings(deviceIds);
    if (normalizedName.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose a name for the device group."));
        return false;
    }
    if (normalizedIds.isEmpty()) {
        setStatusMessage(QStringLiteral("Select at least one device for the group."));
        return false;
    }

    if (!DeviceGroupStore().save(normalizedName, normalizedIds)) {
        setStatusMessage(QStringLiteral("Could not save device group '%1'.").arg(normalizedName));
        return false;
    }

    setStatusMessage(QStringLiteral("Saved device group '%1'.").arg(normalizedName));
    emit deviceGroupsChanged();
    return true;
}

bool AppController::deleteDeviceGroup(const QString& groupName)
{
    const QString normalizedName = normalizeDeviceGroupName(groupName);
    if (normalizedName.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose a device group to delete."));
        return false;
    }

    if (!DeviceGroupStore().remove(normalizedName)) {
        setStatusMessage(QStringLiteral("Device group '%1' does not exist.").arg(normalizedName));
        return false;
    }

    setStatusMessage(QStringLiteral("Deleted device group '%1'.").arg(normalizedName));
    emit deviceGroupsChanged();
    return true;
}

void AppController::syncZoneFrameStreaming(int deviceIndex, int zoneIndex, const RgbEffect& effect)
{
    if (m_deviceManager == nullptr) {
        return;
    }

    const RgbDevice* device = deviceAt(deviceIndex);
    if (effect.isAnimated()
        && device != nullptr
        && device->usesLocalFrameRenderingForEffect(zoneIndex, effect)) {
        m_deviceManager->startZoneFrameStreaming(deviceIndex, zoneIndex);
    } else {
        m_deviceManager->stopZoneFrameStreaming(deviceIndex, zoneIndex);
    }
}

void AppController::setDryRunEnabled(bool enabled)
{
    if (m_deviceManager == nullptr) {
        return;
    }

    if (m_deviceManager->dryRunEnabled() != enabled) {
        m_deviceManager->setDryRunEnabled(enabled);
        // Enabling dry-run means the daemon would refuse streamed host frames via
        // its synchronization guard, so stop streaming instead of silently
        // freezing the animation while every frame is dropped.
        if (enabled) {
            m_deviceManager->stopAllFrameStreaming();
        }
    }

    // Always push the current expectation to the daemon, even when the local
    // value is unchanged, so a drifted daemon can be re-synchronized by
    // re-asserting the setting rather than having to toggle it twice.
    syncDaemonDryRun();
}

bool AppController::applyEffect(int deviceIndex, int zoneIndex, int effectType, const QColor& color, double speed, int brightness)
{
    if (m_deviceManager == nullptr || !color.isValid()) {
        setStatusMessage(QStringLiteral("Could not apply effect."));
        return false;
    }

    if (!isValidEffectType(effectType)) {
        setStatusMessage(QStringLiteral("Unknown effect type."));
        return false;
    }

    if (!zoneSupportsEffect(deviceIndex, zoneIndex, effectType)) {
        setStatusMessage(QStringLiteral("Selected zone does not support this effect."));
        return false;
    }

    const bool speedSupported = zoneSupportsEffectSpeed(deviceIndex, zoneIndex, effectType);
    const bool brightnessSupported = zoneSupportsEffectBrightness(deviceIndex, zoneIndex, effectType);
    const RgbEffect effect(
        static_cast<RgbEffectType>(effectType),
        RgbColor::fromQColor(color),
        speedSupported ? speed : 1.0,
        brightnessSupported ? brightness : 100
    );

    if (auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(deviceAt(deviceIndex))) {
        beginDaemonOperation();
        setStatusMessage(QStringLiteral("Applying effect to selected zone."));
        const QPointer<AppController> self(this);
        const bool dryRunExpected = dryRunEnabled();
        const quint64 requestId = daemonDevice->applyZoneEffectAsync(
            zoneIndex,
            effect,
            dryRunExpected,
            [self, deviceIndex, zoneIndex, dryRunExpected, effect](bool success, const QString& error) {
                if (self == nullptr) {
                    return;
                }
                self->endDaemonOperation();
                self->setStatusMessage(
                    success
                        ? (dryRunExpected
                            ? QStringLiteral("Previewed effect in dry-run.")
                            : QStringLiteral("Applied effect to selected zone."))
                        : (error.isEmpty() ? QStringLiteral("Could not apply effect to selected zone.") : error)
                );
                if (success && !dryRunExpected) {
                    self->syncZoneFrameStreaming(deviceIndex, zoneIndex, effect);
                    emit self->zoneDataChanged(deviceIndex, zoneIndex);
                }
                self->refreshDaemonActivityLog();
            }
        );
        // requestId == 0 means the async call failed immediately and already
        // invoked the handler synchronously (endDaemonOperation + status). Do
        // not repeat that cleanup here; only report the failed return value.
        return requestId != 0;
    }

    const bool changed = m_deviceManager->applyZoneEffect(deviceIndex, zoneIndex, effect);
    if (!changed) {
        const QString hardwareStatus = deviceLastHardwareWriteStatus(deviceIndex);
        setStatusMessage(hardwareStatus.isEmpty() ? QStringLiteral("Could not apply effect to selected zone.") : hardwareStatus);
        emit zoneDataChanged(deviceIndex, zoneIndex);
    } else {
        refreshDaemonActivityLog();
    }

    return changed;
}

int AppController::zoneEffectType(int deviceIndex, int zoneIndex) const
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        return static_cast<int>(RgbEffectType::Static);
    }

    return static_cast<int>(zone->effect().type());
}

QColor AppController::zoneEffectColor(int deviceIndex, int zoneIndex) const
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        return {};
    }

    return zone->effect().color().toQColor();
}

double AppController::zoneEffectSpeed(int deviceIndex, int zoneIndex) const
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        return 1.0;
    }

    return zone->effect().speed();
}

int AppController::zoneEffectBrightness(int deviceIndex, int zoneIndex) const
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        return 100;
    }

    return zone->effect().brightness();
}

bool AppController::zoneEffectsPanelEnabled(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return false;
    }

    return ZoneEffectPreferenceStore().enabled(*device, zoneIndex);
}

void AppController::setZoneEffectsPanelEnabled(int deviceIndex, int zoneIndex, bool enabled)
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return;
    }

    ZoneEffectPreferenceStore().setEnabled(*device, zoneIndex, enabled);
}

bool AppController::updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    if (name.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Zone name cannot be empty."));
        return false;
    }
    if (ledCount < 1) {
        setStatusMessage(QStringLiteral("LED count must be at least 1."));
        return false;
    }

    if (auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(deviceAt(deviceIndex))) {
        beginDaemonOperation();
        setStatusMessage(QStringLiteral("Updating selected zone."));
        const QPointer<AppController> self(this);
        const quint64 requestId = daemonDevice->updateZoneMetadataAsync(
            zoneIndex,
            name,
            ledCount,
            [self, deviceIndex, zoneIndex](bool success, const QString& error) {
                if (self == nullptr) {
                    return;
                }
                self->endDaemonOperation();
                self->setStatusMessage(
                    success
                        ? QStringLiteral("Updated selected zone.")
                        : (error.isEmpty() ? QStringLiteral("Could not update selected zone.") : error)
                );
                emit self->zoneDataChanged(deviceIndex, zoneIndex);
                self->refreshDaemonActivityLog();
            }
        );
        // requestId == 0 means the async call failed immediately and already
        // invoked the handler synchronously; avoid double cleanup here.
        return requestId != 0;
    }

    QString errorMessage;
    const bool updated = m_deviceManager->updateZone(deviceIndex, zoneIndex, name, ledCount, &errorMessage);
    if (!updated && !errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
    } else if (updated) {
        refreshDaemonActivityLog();
    }

    return updated;
}

int AppController::zoneCount(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    return device == nullptr ? 0 : static_cast<int>(device->zones().size());
}

QString AppController::deviceName(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    return device == nullptr ? QString() : device->name();
}

bool AppController::deviceWritable(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return PermissionGate::writeAllowedOrConfirmable(*device);
}

QString AppController::devicePermissionReason(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return {};
    }

    QString fallbackReason;
    for (const BackendCapability capability : {
             BackendCapability::ZoneColorWrite,
             BackendCapability::ZoneEffectWrite,
         }) {
        const PermissionResult permission = device->checkRuntimePermission(capability);
        if (permission.reason.isEmpty()) {
            continue;
        }
        if (permission.status == PermissionStatus::RequiresConfirmation) {
            return permission.reason;
        }
        if (fallbackReason.isEmpty()) {
            fallbackReason = permission.reason;
        }
    }

    return fallbackReason;
}

QString AppController::deviceLastHardwareWriteStatus(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    return device == nullptr ? QString() : device->lastHardwareWriteStatus();
}

bool AppController::deviceSupportsEffect(int deviceIndex, int effectType) const
{
    if (!isValidEffectType(effectType)) {
        return false;
    }

    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return device->supportsEffect(effectType);
}

bool AppController::deviceSupportsEffectSpeed(int deviceIndex, int effectType) const
{
    if (!deviceSupportsEffect(deviceIndex, effectType) || !isAnimatedEffect(effectType)) {
        return false;
    }

    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return device->supportsEffectSpeed(effectType);
}

bool AppController::deviceSupportsEffectBrightness(int deviceIndex, int effectType) const
{
    if (!deviceSupportsEffect(deviceIndex, effectType)) {
        return false;
    }

    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return device->supportsEffectBrightness(effectType);
}

bool AppController::zoneSupportsEffect(int deviceIndex, int zoneIndex, int effectType) const
{
    if (!isValidEffectType(effectType)) {
        return false;
    }

    const RgbDevice* device = deviceAt(deviceIndex);
    return device != nullptr && device->supportsZoneEffect(zoneIndex, effectType);
}

bool AppController::zoneSupportsEffectSpeed(int deviceIndex, int zoneIndex, int effectType) const
{
    if (!zoneSupportsEffect(deviceIndex, zoneIndex, effectType) || !isAnimatedEffect(effectType)) {
        return false;
    }

    const RgbDevice* device = deviceAt(deviceIndex);
    return device != nullptr && device->supportsZoneEffectSpeed(zoneIndex, effectType);
}

bool AppController::zoneSupportsEffectBrightness(int deviceIndex, int zoneIndex, int effectType) const
{
    if (!zoneSupportsEffect(deviceIndex, zoneIndex, effectType)) {
        return false;
    }

    const RgbDevice* device = deviceAt(deviceIndex);
    return device != nullptr && device->supportsZoneEffectBrightness(zoneIndex, effectType);
}

bool AppController::globalTargetSupportsEffect(const QString& groupName, int effectType) const
{
    if (m_deviceManager == nullptr || !isValidEffectType(effectType)) {
        return false;
    }

    const QString normalizedName = normalizeDeviceGroupName(groupName);
    const QStringList targetDeviceIds = normalizedName.isEmpty()
        ? QStringList {}
        : storedDeviceGroupIds(normalizedName);
    if (!normalizedName.isEmpty() && targetDeviceIds.isEmpty()) {
        return false;
    }

    const QSet<QString> targetIdSet(targetDeviceIds.cbegin(), targetDeviceIds.cend());
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = deviceAt(deviceIndex);
        if (device == nullptr || !deviceWritable(deviceIndex)) {
            continue;
        }
        if (!targetIdSet.isEmpty() && !targetIdSet.contains(device->id())) {
            continue;
        }

        for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
            if (zoneSupportsEffect(deviceIndex, zoneIndex, effectType)) {
                return true;
            }
        }
    }

    return false;
}

bool AppController::globalTargetSupportsEffectSpeed(const QString& groupName, int effectType) const
{
    if (m_deviceManager == nullptr || !isAnimatedEffect(effectType) || !isValidEffectType(effectType)) {
        return false;
    }

    const QString normalizedName = normalizeDeviceGroupName(groupName);
    const QStringList targetDeviceIds = normalizedName.isEmpty()
        ? QStringList {}
        : storedDeviceGroupIds(normalizedName);
    if (!normalizedName.isEmpty() && targetDeviceIds.isEmpty()) {
        return false;
    }

    const QSet<QString> targetIdSet(targetDeviceIds.cbegin(), targetDeviceIds.cend());
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = deviceAt(deviceIndex);
        if (device == nullptr || !deviceWritable(deviceIndex)) {
            continue;
        }
        if (!targetIdSet.isEmpty() && !targetIdSet.contains(device->id())) {
            continue;
        }

        for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
            if (zoneSupportsEffectSpeed(deviceIndex, zoneIndex, effectType)) {
                return true;
            }
        }
    }

    return false;
}

bool AppController::globalTargetSupportsEffectBrightness(const QString& groupName, int effectType) const
{
    if (m_deviceManager == nullptr || !isValidEffectType(effectType)) {
        return false;
    }

    const QString normalizedName = normalizeDeviceGroupName(groupName);
    const QStringList targetDeviceIds = normalizedName.isEmpty()
        ? QStringList {}
        : storedDeviceGroupIds(normalizedName);
    if (!normalizedName.isEmpty() && targetDeviceIds.isEmpty()) {
        return false;
    }

    const QSet<QString> targetIdSet(targetDeviceIds.cbegin(), targetDeviceIds.cend());
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = deviceAt(deviceIndex);
        if (device == nullptr || !deviceWritable(deviceIndex)) {
            continue;
        }
        if (!targetIdSet.isEmpty() && !targetIdSet.contains(device->id())) {
            continue;
        }

        for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
            if (zoneSupportsEffectBrightness(deviceIndex, zoneIndex, effectType)) {
                return true;
            }
        }
    }

    return false;
}

bool AppController::deviceRequiresConfirmation(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return PermissionGate::writeRequiresConfirmation(*device);
}

bool AppController::deviceWriteConfirmed(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    if (const auto* daemonDevice = dynamic_cast<const DaemonRgbDevice*>(device)) {
        return daemonDevice->writeConfirmed();
    }

    return m_deviceManager->deviceWriteConfirmed(deviceIndex);
}

bool AppController::confirmDeviceWrites(int deviceIndex)
{
    if (m_daemonClient == nullptr || !m_daemonClient->isConnected()) {
        setStatusMessage(QStringLiteral("Could not confirm hardware writes: daemon is not connected."));
        return false;
    }

    beginDaemonOperation();
    setStatusMessage(QStringLiteral("Confirming hardware writes."));
    const QPointer<AppController> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::ConfirmWrites),
        {{QStringLiteral("deviceIndex"), deviceIndex}},
        [self, deviceIndex](DaemonCallResult response) {
            if (self == nullptr) {
                return;
            }
            self->endDaemonOperation();
            const bool success =
                response.ok && response.result.value(QStringLiteral("success")).toBool(false);
            if (!success) {
                self->setStatusMessage(
                    response.ok ? QStringLiteral("Could not confirm hardware writes.") : response.error
                );
            } else {
                self->setStatusMessage(QStringLiteral("Hardware writes confirmed for this daemon session."));
                self->setLocalDaemonWriteConfirmed(
                    deviceIndex,
                    response.result.value(QStringLiteral("writeConfirmed")).toBool(true)
                );
                emit self->backendInfoChanged();
            }
            self->refreshDaemonActivityLog();
        }
    );
    Q_UNUSED(requestId)
    return true;
}

bool AppController::revokeDeviceWrites(int deviceIndex)
{
    if (m_daemonClient == nullptr || !m_daemonClient->isConnected()) {
        setStatusMessage(QStringLiteral("Could not revoke hardware writes: daemon is not connected."));
        return false;
    }

    beginDaemonOperation();
    setStatusMessage(QStringLiteral("Revoking hardware write confirmation."));
    const QPointer<AppController> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::RevokeWrites),
        {{QStringLiteral("deviceIndex"), deviceIndex}},
        [self, deviceIndex](DaemonCallResult response) {
            if (self == nullptr) {
                return;
            }
            self->endDaemonOperation();
            const bool success =
                response.ok && response.result.value(QStringLiteral("success")).toBool(false);
            if (!success) {
                self->setStatusMessage(
                    response.ok ? QStringLiteral("Could not revoke hardware writes.") : response.error
                );
            } else {
                self->setStatusMessage(QStringLiteral("Hardware write confirmation revoked."));
                self->setLocalDaemonWriteConfirmed(
                    deviceIndex,
                    response.result.value(QStringLiteral("writeConfirmed")).toBool(false)
                );
                emit self->backendInfoChanged();
            }
            self->refreshDaemonActivityLog();
        }
    );
    Q_UNUSED(requestId)
    return true;
}

bool AppController::allOffDevice(int deviceIndex)
{
    if (m_deviceManager == nullptr) {
        setStatusMessage(QStringLiteral("Could not turn device off."));
        return false;
    }

    if (auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(deviceAt(deviceIndex))) {
        beginDaemonOperation();
        setStatusMessage(QStringLiteral("Turning off selected device."));
        const QPointer<AppController> self(this);
        const bool dryRunExpected = dryRunEnabled();
        const quint64 requestId = daemonDevice->applyAllOffAsync(
            dryRunExpected,
            [self, deviceIndex, dryRunExpected](bool success, const QString& error) {
                if (self == nullptr) {
                    return;
                }
                self->endDaemonOperation();
                self->setStatusMessage(
                    success
                        ? (dryRunExpected
                            ? QStringLiteral("Previewed All Off in dry-run.")
                            : QStringLiteral("Sent all-off command to selected device."))
                        : (error.isEmpty() ? QStringLiteral("Could not turn device off.") : error)
                );
                if (success && !dryRunExpected) {
                    // All Off turns the device off, so any host-streamed animation
                    // for this device must stop or it keeps pushing frames forever.
                    if (self->m_deviceManager != nullptr) {
                        self->m_deviceManager->stopDeviceFrameStreaming(deviceIndex);
                    }
                    emit self->zoneDataChanged(deviceIndex, -1);
                }
                self->refreshDaemonActivityLog();
            }
        );
        // requestId == 0 means the async call failed immediately and already
        // invoked the handler synchronously; avoid double cleanup here.
        return requestId != 0;
    }

    QString error;
    if (!m_deviceManager->applyAllOff(deviceIndex, &error)) {
        setStatusMessage(error.isEmpty() ? QStringLiteral("Could not turn device off.") : error);
        refreshDaemonActivityLog();
        return false;
    }

    setStatusMessage(QStringLiteral("Sent all-off command to selected device."));
    refreshDaemonActivityLog();
    emit zoneDataChanged(deviceIndex, -1);
    return true;
}

bool AppController::applyEffectGlobally(
    int effectType,
    const QColor& color,
    double speed,
    int brightness
)
{
    return applyGlobalEffectInternal(effectType, color, speed, brightness, false, {}, QStringLiteral("All devices"));
}

bool AppController::applyEffectToDeviceGroup(
    const QString& groupName,
    int effectType,
    const QColor& color,
    double speed,
    int brightness
)
{
    const QString normalizedName = normalizeDeviceGroupName(groupName);
    const QStringList targetDeviceIds = storedDeviceGroupIds(normalizedName);
    if (normalizedName.isEmpty() || targetDeviceIds.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose a non-empty device group."));
        return false;
    }

    return applyGlobalEffectInternal(effectType, color, speed, brightness, false, targetDeviceIds, normalizedName);
}

bool AppController::setZoneBrightness(int deviceIndex, int zoneIndex, int brightness)
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        setStatusMessage(QStringLiteral("Select a zone before changing brightness."));
        return false;
    }

    if (brightness < 0 || brightness > 100) {
        setStatusMessage(QStringLiteral("Brightness must be between 0 and 100 percent."));
        return false;
    }

    const int effectType = static_cast<int>(zone->effect().type());
    if (!zoneSupportsEffectBrightness(deviceIndex, zoneIndex, effectType)) {
        setStatusMessage(QStringLiteral("Selected zone does not expose brightness for its current effect."));
        return false;
    }

    return applyEffect(
        deviceIndex,
        zoneIndex,
        effectType,
        zone->effect().color().toQColor(),
        zone->effect().speed(),
        brightness
    );
}

bool AppController::setGlobalBrightness(int brightness)
{
    return applyGlobalEffectInternal(
        static_cast<int>(RgbEffectType::Static),
        QColor(Qt::black),
        1.0,
        brightness,
        true,
        {},
        QStringLiteral("All devices")
    );
}

bool AppController::setDeviceGroupBrightness(const QString& groupName, int brightness)
{
    const QString normalizedName = normalizeDeviceGroupName(groupName);
    const QStringList targetDeviceIds = storedDeviceGroupIds(normalizedName);
    if (normalizedName.isEmpty() || targetDeviceIds.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose a non-empty device group."));
        return false;
    }

    return applyGlobalEffectInternal(
        static_cast<int>(RgbEffectType::Static),
        QColor(Qt::black),
        1.0,
        brightness,
        true,
        targetDeviceIds,
        normalizedName
    );
}

bool AppController::applyGlobalEffectInternal(
    int effectType,
    const QColor& color,
    double speed,
    int brightness,
    bool preserveCurrentEffect,
    const QStringList& targetDeviceIds,
    const QString& targetName
)
{
    if (m_deviceManager == nullptr
        || (!preserveCurrentEffect && (!isValidEffectType(effectType) || !color.isValid()))) {
        setStatusMessage(QStringLiteral("Could not start global lighting operation."));
        return false;
    }

    const QStringList normalizedTargetIds = uniqueNonEmptyStrings(targetDeviceIds);
    QSet<QString> targetIdSet;
    for (const QString& deviceId : normalizedTargetIds) {
        targetIdSet.insert(deviceId);
    }
    const bool groupScoped = !normalizedTargetIds.isEmpty();
    auto state = std::make_shared<GlobalOperationState>();
    state->operation = preserveCurrentEffect
        ? (groupScoped ? QStringLiteral("Group brightness") : QStringLiteral("Global brightness"))
        : (groupScoped ? QStringLiteral("Group effect") : QStringLiteral("Global effect"));
    state->targetName = targetName.isEmpty() ? QStringLiteral("All devices") : targetName;
    state->targetKind = groupScoped ? QStringLiteral("group") : QStringLiteral("all");
    const QPointer<AppController> self(this);
    const auto finish = std::make_shared<std::function<void()>>();
    *finish = [self, state]() {
        if (self == nullptr || state->dispatching || state->pending > 0) {
            return;
        }

        const QVariantMap result {
            {QStringLiteral("operation"), state->operation},
            {QStringLiteral("total"), state->total},
            {QStringLiteral("applied"), state->applied},
            {QStringLiteral("skipped"), state->skipped},
            {QStringLiteral("failed"), state->failed},
            {QStringLiteral("partial"), state->applied > 0 && (state->skipped > 0 || state->failed > 0)},
            {QStringLiteral("targetName"), state->targetName},
            {QStringLiteral("targetKind"), state->targetKind},
            {QStringLiteral("details"), state->details},
        };
        self->setStatusMessage(
            state->failed == 0 && state->skipped == 0
                ? QStringLiteral("%1 completed for %2 target(s).").arg(state->operation).arg(state->applied)
                : QStringLiteral("%1 completed: %2 applied, %3 skipped, %4 failed.")
                      .arg(state->operation)
                      .arg(state->applied)
                      .arg(state->skipped)
                      .arg(state->failed)
        );
        self->refreshDaemonActivityLog();
        emit self->globalOperationFinished(result);
    };

    const int boundedBrightness = qBound(0, brightness, 100);
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        RgbDevice* device = deviceAt(deviceIndex);
        if (device != nullptr && groupScoped && !targetIdSet.contains(device->id())) {
            continue;
        }
        if (device == nullptr || !deviceWritable(deviceIndex)) {
            continue;
        }

        for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
            ++state->total;
            RgbEffect effect = preserveCurrentEffect
                ? device->zoneEffect(zoneIndex)
                : RgbEffect(
                      static_cast<RgbEffectType>(effectType),
                      RgbColor::fromQColor(color),
                      speed,
                      boundedBrightness
                  );
            if (preserveCurrentEffect) {
                effect.setBrightness(boundedBrightness);
            }

            const int currentEffectType = static_cast<int>(effect.type());
            if (!zoneSupportsEffect(deviceIndex, zoneIndex, currentEffectType)) {
                ++state->skipped;
                state->details.append(
                    QStringLiteral("%1 / %2: effect is not supported.")
                        .arg(device->name(), device->zones().at(zoneIndex).name())
                );
                continue;
            }
            if (preserveCurrentEffect
                && !zoneSupportsEffectBrightness(deviceIndex, zoneIndex, currentEffectType)) {
                ++state->skipped;
                state->details.append(
                    QStringLiteral("%1 / %2: brightness control is not supported.")
                        .arg(device->name(), device->zones().at(zoneIndex).name())
                );
                continue;
            }
            if (!preserveCurrentEffect) {
                if (!zoneSupportsEffectSpeed(deviceIndex, zoneIndex, currentEffectType)) {
                    effect.setSpeed(1.0);
                }
                if (!zoneSupportsEffectBrightness(deviceIndex, zoneIndex, currentEffectType)) {
                    effect.setBrightness(100);
                }
            }
            if (!dryRunEnabled()
                && deviceRequiresConfirmation(deviceIndex)
                && !deviceWriteConfirmed(deviceIndex)) {
                ++state->skipped;
                state->details.append(
                    QStringLiteral("%1 / %2: hardware writes are not confirmed.")
                        .arg(device->name(), device->zones().at(zoneIndex).name())
                );
                continue;
            }

            if (auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(device)) {
                ++state->pending;
                beginDaemonOperation();
                const bool dryRunExpected = dryRunEnabled();
                const quint64 requestId = daemonDevice->applyZoneEffectAsync(
                    zoneIndex,
                    effect,
                    dryRunExpected,
                    [self, state, finish, deviceIndex, zoneIndex, dryRunExpected, effect](bool success, const QString& error) {
                        if (self == nullptr) {
                            return;
                        }
                        self->endDaemonOperation();
                        --state->pending;
                        if (success) {
                            ++state->applied;
                        } else {
                            ++state->failed;
                            state->details.append(
                                error.isEmpty()
                                    ? QStringLiteral("Device %1 / zone %2: write failed.")
                                          .arg(deviceIndex)
                                          .arg(zoneIndex)
                                : error
                            );
                        }
                        if (success && !dryRunExpected) {
                            self->syncZoneFrameStreaming(deviceIndex, zoneIndex, effect);
                            emit self->zoneDataChanged(deviceIndex, zoneIndex);
                        }
                        (*finish)();
                    }
                );
                Q_UNUSED(requestId)
                continue;
            }

            if (m_deviceManager->applyZoneEffect(deviceIndex, zoneIndex, effect)) {
                ++state->applied;
            } else {
                ++state->failed;
                state->details.append(
                    QStringLiteral("%1 / %2: write failed.")
                        .arg(device->name(), device->zones().at(zoneIndex).name())
                );
            }
        }
    }

    state->dispatching = false;
    if (state->total == 0) {
        setStatusMessage(
            groupScoped
                ? QStringLiteral("No writable zones are available in '%1'.").arg(state->targetName)
                : QStringLiteral("No writable zones are available.")
        );
        return false;
    }

    setStatusMessage(QStringLiteral("%1 started for %2 target(s).").arg(state->operation).arg(state->total));
    (*finish)();
    return true;
}

bool AppController::allOffAllDevices()
{
    return allOffDevicesInternal({}, QStringLiteral("All devices"));
}

bool AppController::allOffDeviceGroup(const QString& groupName)
{
    const QString normalizedName = normalizeDeviceGroupName(groupName);
    const QStringList targetDeviceIds = storedDeviceGroupIds(normalizedName);
    if (normalizedName.isEmpty() || targetDeviceIds.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose a non-empty device group."));
        return false;
    }

    return allOffDevicesInternal(targetDeviceIds, normalizedName);
}

bool AppController::allOffDevicesInternal(const QStringList& targetDeviceIds, const QString& targetName)
{
    if (m_deviceManager == nullptr) {
        setStatusMessage(QStringLiteral("Could not start global All Off."));
        return false;
    }

    const QStringList normalizedTargetIds = uniqueNonEmptyStrings(targetDeviceIds);
    QSet<QString> targetIdSet;
    for (const QString& deviceId : normalizedTargetIds) {
        targetIdSet.insert(deviceId);
    }
    const bool groupScoped = !normalizedTargetIds.isEmpty();
    auto state = std::make_shared<GlobalOperationState>();
    state->operation = groupScoped ? QStringLiteral("Group All Off") : QStringLiteral("All Off");
    state->targetName = targetName.isEmpty() ? QStringLiteral("All devices") : targetName;
    state->targetKind = groupScoped ? QStringLiteral("group") : QStringLiteral("all");
    const QPointer<AppController> self(this);
    const auto finish = std::make_shared<std::function<void()>>();
    *finish = [self, state]() {
        if (self == nullptr || state->dispatching || state->pending > 0) {
            return;
        }

        const QVariantMap result {
            {QStringLiteral("operation"), state->operation},
            {QStringLiteral("total"), state->total},
            {QStringLiteral("applied"), state->applied},
            {QStringLiteral("skipped"), state->skipped},
            {QStringLiteral("failed"), state->failed},
            {QStringLiteral("partial"), state->applied > 0 && (state->skipped > 0 || state->failed > 0)},
            {QStringLiteral("targetName"), state->targetName},
            {QStringLiteral("targetKind"), state->targetKind},
            {QStringLiteral("details"), state->details},
        };
        self->setStatusMessage(
            state->failed == 0 && state->skipped == 0
                ? QStringLiteral("All Off completed for %1 device(s).").arg(state->applied)
                : QStringLiteral("All Off completed: %1 applied, %2 skipped, %3 failed.")
                      .arg(state->applied)
                      .arg(state->skipped)
                      .arg(state->failed)
        );
        self->refreshDaemonActivityLog();
        emit self->globalOperationFinished(result);
    };

    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        RgbDevice* device = deviceAt(deviceIndex);
        if (device != nullptr && groupScoped && !targetIdSet.contains(device->id())) {
            continue;
        }
        if (device == nullptr || !deviceWritable(deviceIndex)) {
            continue;
        }

        ++state->total;
        if (!dryRunEnabled()
            && deviceRequiresConfirmation(deviceIndex)
            && !deviceWriteConfirmed(deviceIndex)) {
            ++state->skipped;
            state->details.append(
                QStringLiteral("%1: hardware writes are not confirmed.").arg(device->name())
            );
            continue;
        }

        if (auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(device)) {
            ++state->pending;
            beginDaemonOperation();
            const bool dryRunExpected = dryRunEnabled();
            const quint64 requestId = daemonDevice->applyAllOffAsync(
                dryRunExpected,
                [self, state, finish, deviceIndex, dryRunExpected](bool success, const QString& error) {
                    if (self == nullptr) {
                        return;
                    }
                    self->endDaemonOperation();
                    --state->pending;
                    if (success) {
                        ++state->applied;
                    } else {
                        ++state->failed;
                        state->details.append(
                            error.isEmpty()
                                ? QStringLiteral("Device %1: All Off failed.").arg(deviceIndex)
                                : error
                        );
                    }
                    if (success && !dryRunExpected) {
                        if (self->m_deviceManager != nullptr) {
                            self->m_deviceManager->stopDeviceFrameStreaming(deviceIndex);
                        }
                        emit self->zoneDataChanged(deviceIndex, -1);
                    }
                    (*finish)();
                }
            );
            Q_UNUSED(requestId)
            continue;
        }

        QString error;
        if (m_deviceManager->applyAllOff(deviceIndex, &error)) {
            ++state->applied;
            emit zoneDataChanged(deviceIndex, -1);
        } else {
            ++state->failed;
            state->details.append(
                error.isEmpty()
                    ? QStringLiteral("%1: All Off failed.").arg(device->name())
                    : error
            );
        }
    }

    state->dispatching = false;
    if (state->total == 0) {
        setStatusMessage(
            groupScoped
                ? QStringLiteral("No writable devices are available in '%1'.").arg(state->targetName)
                : QStringLiteral("No writable devices are available.")
        );
        return false;
    }

    setStatusMessage(QStringLiteral("All Off started for %1 device(s).").arg(state->total));
    (*finish)();
    return true;
}

bool AppController::markDeviceRgbController(int deviceIndex)
{
    if (m_deviceManager == nullptr || !m_deviceManager->markDeviceRgbController(deviceIndex, true)) {
        setStatusMessage(QStringLiteral("Could not register selected device as an RGB controller."));
        return false;
    }

    setStatusMessage(QStringLiteral("Registered '%1' as an RGB controller.").arg(deviceName(deviceIndex)));
    refreshDaemonActivityLog();
    return true;
}

bool AppController::removeDeviceRgbController(int deviceIndex)
{
    if (m_deviceManager == nullptr || !m_deviceManager->markDeviceRgbController(deviceIndex, false)) {
        setStatusMessage(QStringLiteral("Could not remove selected device from RGB controllers."));
        return false;
    }

    setStatusMessage(QStringLiteral("Removed '%1' from RGB controllers.").arg(deviceName(deviceIndex)));
    refreshDaemonActivityLog();
    return true;
}

bool AppController::resetDeviceRgbControllerOverride(int deviceIndex)
{
    if (m_deviceManager == nullptr || !m_deviceManager->clearDeviceRgbControllerOverride(deviceIndex)) {
        setStatusMessage(QStringLiteral("Could not reset RGB controller override."));
        return false;
    }

    setStatusMessage(QStringLiteral("Reset RGB controller override for '%1'.").arg(deviceName(deviceIndex)));
    refreshDaemonActivityLog();
    return true;
}

QString AppController::deviceId(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    return device == nullptr ? QString() : device->id();
}

int AppController::deviceIndexForId(const QString& deviceId) const
{
    if (m_deviceManager == nullptr || deviceId.isEmpty()) {
        return -1;
    }

    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
        if (device != nullptr && device->id() == deviceId) {
            return deviceIndex;
        }
    }
    return -1;
}

int AppController::zoneIndexForName(int deviceIndex, const QString& zoneName) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr || zoneName.isEmpty()) {
        return -1;
    }

    for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
        if (device->zones().at(zoneIndex).name() == zoneName) {
            return zoneIndex;
        }
    }
    return -1;
}

QString AppController::zoneName(int deviceIndex, int zoneIndex) const
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        return {};
    }

    return zone->name();
}

int AppController::zoneLedCount(int deviceIndex, int zoneIndex) const
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        return 0;
    }

    return zone->ledCount();
}

QString AppController::zoneColorHex(int deviceIndex, int zoneIndex) const
{
    const RgbZone* zone = zoneAt(deviceIndex, zoneIndex);
    if (zone == nullptr) {
        return {};
    }

    return zone->currentColor().toHexString();
}

bool AppController::saveProfile(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool saved = m_deviceManager->saveProfile(profileName, &errorMessage);
    if (!saved && !errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
    } else if (saved) {
        emit profilesChanged();
    }

    return saved;
}

bool AppController::loadProfile(const QString& profileName)
{
    return applyProfileWithReport(profileName).value(QStringLiteral("success")).toBool();
}

QVariantMap AppController::applyProfileWithReport(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    const QVariantMap report = m_deviceManager->applyProfileWithReport(profileName);
    const QString message = report.value(QStringLiteral("success")).toBool()
        ? report.value(QStringLiteral("summary")).toString()
        : report.value(QStringLiteral("error")).toString();
    if (!message.isEmpty()) {
        setStatusMessage(message);
    }
    refreshDaemonActivityLog();

    return report;
}

bool AppController::applyProfileAsync(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }
    if (m_profileApplyInProgress) {
        setStatusMessage(QStringLiteral("A profile is already being applied."));
        return false;
    }

    const QString normalizedName = ProfileStore::normalizeName(profileName);
    ProfileStore profileStore(m_deviceManager->profilesDirectoryPath());
    QJsonObject profile;
    QString storeError;
    if (!profileStore.load(normalizedName, &profile, &storeError)) {
        m_deviceManager->activityLog().error(LogCategory::Profile, storeError);
        setStatusMessage(storeError);
        emit profileApplyFinished(QVariantMap {
            {QStringLiteral("success"), false},
            {QStringLiteral("partial"), false},
            {QStringLiteral("profileName"), normalizedName},
            {QStringLiteral("error"), storeError},
        });
        return false;
    }

    auto state = std::make_shared<ProfileApplyState>();
    QVector<ProfileDeviceRef> deviceRefs;
    deviceRefs.reserve(m_deviceManager->deviceCount());
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        deviceRefs.append(ProfileDeviceRef {deviceIndex, deviceAt(deviceIndex)});
    }
    const ProfileApplyPlan plan = buildProfileApplyPlan(normalizedName, profile, deviceRefs);
    state->summary = plan.summary;

    m_profileApplyInProgress = true;
    emit profileApplyInProgressChanged();
    setStatusMessage(QStringLiteral("Applying profile '%1'.").arg(normalizedName));

    const QPointer<AppController> self(this);
    const auto finish = std::make_shared<std::function<void()>>();
    *finish = [self, state]() {
        if (self == nullptr || state->dispatching || state->pending > 0) {
            return;
        }

        const QVariantMap report = profileApplyReport(state->summary);
        const bool success = report.value(QStringLiteral("success")).toBool();
        if (success) {
            self->m_deviceManager->activityLog().info(
                LogCategory::Profile,
                QStringLiteral("Loaded profile '%1'. %2")
                    .arg(state->summary.profileName, report.value(QStringLiteral("summary")).toString())
            );
        } else {
            self->m_deviceManager->activityLog().error(
                LogCategory::Profile,
                report.value(QStringLiteral("summary")).toString()
            );
        }

        self->m_profileApplyInProgress = false;
        emit self->profileApplyInProgressChanged();
        self->setStatusMessage(
            success
                ? report.value(QStringLiteral("summary")).toString()
                : report.value(QStringLiteral("error")).toString()
        );
        self->refreshDaemonActivityLog();
        emit self->profileApplyFinished(report);
    };

    for (const ProfileApplyStep& step : plan.steps) {
        if (step.kind == ProfileApplyStepKind::Skip) {
            appendProfileApplySkip(&state->summary, step.skipReason, step.skippedZoneCount, step.detail);
            continue;
        }

        const ProfileApplyTarget& target = step.target;
        RgbDevice* device = deviceAt(target.deviceIndex);
        if (device == nullptr) {
            ++state->summary.failedZones;
            state->summary.details.append(QStringLiteral("Failed to apply zone: %1 / %2").arg(target.deviceName, target.zoneName));
            continue;
        }

        if (auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(device)) {
            const QPointer<DaemonRgbDevice> daemonDevicePointer(daemonDevice);
            ++state->pending;
            beginDaemonOperation();
            const quint64 metadataRequestId = daemonDevice->updateZoneMetadataAsync(
                target.zoneIndex,
                target.zoneName,
                target.ledCount,
                [self, state, finish, target, daemonDevicePointer](bool metadataSuccess, const QString& metadataError) {
                    if (self == nullptr) {
                        return;
                    }
                    self->endDaemonOperation();
                    if (!metadataSuccess) {
                        ++state->summary.failedZones;
                        state->summary.details.append(
                            metadataError.isEmpty()
                                ? QStringLiteral("Failed to apply zone: %1 / %2").arg(target.deviceName, target.zoneName)
                                : metadataError
                        );
                        --state->pending;
                        (*finish)();
                        return;
                    }
                    if (daemonDevicePointer == nullptr) {
                        ++state->summary.failedZones;
                        state->summary.details.append(QStringLiteral("Failed to apply zone: %1 / %2").arg(target.deviceName, target.zoneName));
                        --state->pending;
                        (*finish)();
                        return;
                    }

                    self->beginDaemonOperation();
                    const bool dryRunExpected = self->dryRunEnabled();
                    const quint64 effectRequestId = daemonDevicePointer->applyZoneEffectAsync(
                        target.zoneIndex,
                        target.effect,
                        dryRunExpected,
                        [self, state, finish, target, dryRunExpected](bool effectSuccess, const QString& effectError) {
                            if (self == nullptr) {
                                return;
                            }
                            self->endDaemonOperation();
                            --state->pending;
                            if (effectSuccess) {
                                ++state->summary.appliedZones;
                            } else {
                                ++state->summary.failedZones;
                                state->summary.details.append(
                                    effectError.isEmpty()
                                        ? QStringLiteral("Failed to apply zone: %1 / %2").arg(target.deviceName, target.zoneName)
                                        : effectError
                                );
                            }
                            if (effectSuccess && !dryRunExpected) {
                                self->syncZoneFrameStreaming(target.deviceIndex, target.zoneIndex, target.effect);
                                emit self->zoneDataChanged(target.deviceIndex, target.zoneIndex);
                            }
                            (*finish)();
                        }
                    );
                    // effectRequestId == 0 means applyZoneEffectAsync failed
                    // immediately and already invoked the effect handler above
                    // (endDaemonOperation, --pending, finish). Nothing to do.
                    Q_UNUSED(effectRequestId)
                }
            );
            // metadataRequestId == 0 means updateZoneMetadataAsync failed
            // immediately and already invoked the metadata handler above
            // (endDaemonOperation, --pending, finish). Nothing to do.
            Q_UNUSED(metadataRequestId)
            continue;
        }

        if (!m_deviceManager->updateZone(target.deviceIndex, target.zoneIndex, target.zoneName, target.ledCount)
            || !m_deviceManager->applyZoneEffect(target.deviceIndex, target.zoneIndex, target.effect)) {
            ++state->summary.failedZones;
            state->summary.details.append(QStringLiteral("Failed to apply zone: %1 / %2").arg(target.deviceName, target.zoneName));
            continue;
        }
        ++state->summary.appliedZones;
    }

    state->dispatching = false;
    (*finish)();
    return true;
}

bool AppController::deleteProfile(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool deleted = m_deviceManager->deleteProfile(profileName, &errorMessage);
    if (!deleted && !errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
    } else if (deleted) {
        emit profilesChanged();
    }

    return deleted;
}

bool AppController::renameProfile(const QString& oldProfileName, const QString& newProfileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool renamed = m_deviceManager->renameProfile(oldProfileName, newProfileName, &errorMessage);
    if (!renamed && !errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
    } else if (renamed) {
        emit profilesChanged();
    }

    return renamed;
}

QString AppController::importProfile(const QUrl& sourceUrl)
{
    if (m_deviceManager == nullptr || !sourceUrl.isLocalFile()) {
        setStatusMessage(QStringLiteral("Choose a local JSON file to import."));
        return {};
    }

    QString importedProfileName;
    QString errorMessage;
    if (!m_deviceManager->importProfile(sourceUrl.toLocalFile(), &importedProfileName, &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not import profile.") : errorMessage);
        return {};
    }

    setStatusMessage(QStringLiteral("Imported profile '%1'.").arg(importedProfileName));
    emit profilesChanged();
    return importedProfileName;
}

bool AppController::exportProfile(const QString& profileName, const QUrl& destinationUrl)
{
    if (m_deviceManager == nullptr || !destinationUrl.isLocalFile()) {
        setStatusMessage(QStringLiteral("Choose a local destination for the profile export."));
        return false;
    }

    QString destinationPath = destinationUrl.toLocalFile();
    if (QFileInfo(destinationPath).suffix().isEmpty()) {
        destinationPath.append(QStringLiteral(".json"));
    }

    QString errorMessage;
    if (!m_deviceManager->exportProfile(profileName, destinationPath, &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not export profile.") : errorMessage);
        return false;
    }

    setStatusMessage(QStringLiteral("Exported profile '%1'.").arg(profileName));
    refreshDaemonActivityLog();
    return true;
}

QVariantMap AppController::diagnosticsReport() const
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    const QString profilesDirectory = m_deviceManager->profilesDirectoryPath();
    const auto sanitize = [&profilesDirectory](const QString& value) {
        return sanitizedDiagnosticString(value, profilesDirectory);
    };

    const BackendDescriptor backend = m_deviceManager->backendRegistry().activeDescriptor();
    const QString effectiveBackendId = backendEffectiveId();
    QVariantList backendCapabilities;
    for (const BackendCapability capability : {
             BackendCapability::DiscoveryRead,
             BackendCapability::ZoneColorWrite,
             BackendCapability::ZoneEffectWrite,
         }) {
        if (backend.capabilities.testFlag(capability)) {
            backendCapabilities.append(backendCapabilityToString(capability));
        }
    }

    QVariantList profileNames;
    for (const QString& profileName : m_deviceManager->profileNames()) {
        profileNames.append(profileName);
    }

    int writableDeviceCount = 0;
    int readOnlyDeviceCount = 0;
    int confirmationRequiredDeviceCount = 0;
    int confirmedDeviceCount = 0;
    int totalZoneCount = 0;

    QVariantList devices;
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
        if (device == nullptr) {
            continue;
        }

        const bool writable = deviceWritable(deviceIndex);
        const bool requiresConfirmation = deviceRequiresConfirmation(deviceIndex);
        const bool writeConfirmed = deviceWriteConfirmed(deviceIndex);
        if (writable) {
            ++writableDeviceCount;
        } else {
            ++readOnlyDeviceCount;
        }
        if (requiresConfirmation && !writeConfirmed) {
            ++confirmationRequiredDeviceCount;
        }
        if (writeConfirmed) {
            ++confirmedDeviceCount;
        }

        QVariantList deviceCapabilities;
        for (const BackendCapability capability : {
                 BackendCapability::DiscoveryRead,
                 BackendCapability::ZoneColorWrite,
                 BackendCapability::ZoneEffectWrite,
             }) {
            if (device->capabilities().testFlag(capability)) {
                deviceCapabilities.append(backendCapabilityToString(capability));
            }
        }

        QVariantMap permissions {
            {
                backendCapabilityToString(BackendCapability::DiscoveryRead),
                permissionToDiagnostics(device->checkRuntimePermission(BackendCapability::DiscoveryRead), sanitize),
            },
            {
                backendCapabilityToString(BackendCapability::ZoneColorWrite),
                permissionToDiagnostics(device->checkRuntimePermission(BackendCapability::ZoneColorWrite), sanitize),
            },
            {
                backendCapabilityToString(BackendCapability::ZoneEffectWrite),
                permissionToDiagnostics(device->checkRuntimePermission(BackendCapability::ZoneEffectWrite), sanitize),
            },
        };

        const auto supportedEffects = [device](int zoneIndex) {
            QVariantList effects;
            for (const RgbEffectType effectType : allRgbEffectTypes()) {
                const int effectTypeValue = static_cast<int>(effectType);
                const bool zoneScoped = zoneIndex >= 0;
                effects.append(QVariantMap {
                    {QStringLiteral("type"), rgbEffectTypeToString(effectType)},
                    {QStringLiteral("supported"), zoneScoped
                         ? device->supportsZoneEffect(zoneIndex, effectTypeValue)
                         : device->supportsEffect(effectTypeValue)},
                    {QStringLiteral("speed"), zoneScoped
                         ? device->supportsZoneEffectSpeed(zoneIndex, effectTypeValue)
                         : device->supportsEffectSpeed(effectTypeValue)},
                    {QStringLiteral("brightness"), zoneScoped
                         ? device->supportsZoneEffectBrightness(zoneIndex, effectTypeValue)
                         : device->supportsEffectBrightness(effectTypeValue)},
                });
            }
            return effects;
        };

        const QVariantList effects = supportedEffects(-1);

        QVariantList zones;
        const QVector<RgbZone>& deviceZones = device->zones();
        totalZoneCount += deviceZones.size();
        for (int zoneIndex = 0; zoneIndex < deviceZones.size(); ++zoneIndex) {
            const RgbZone& zone = deviceZones.at(zoneIndex);
            zones.append(QVariantMap {
                {QStringLiteral("index"), zoneIndex},
                {QStringLiteral("name"), sanitize(zone.name())},
                {QStringLiteral("type"), zone.typeName()},
                {QStringLiteral("ledCount"), zone.ledCount()},
                {QStringLiteral("currentColor"), zone.currentColor().toHexString()},
                {QStringLiteral("effect"), effectToDiagnostics(zone.effect())},
                {QStringLiteral("supportedEffects"), supportedEffects(zoneIndex)},
            });
        }

        devices.append(QVariantMap {
            {QStringLiteral("index"), deviceIndex},
            {QStringLiteral("id"), sanitize(device->id())},
            {QStringLiteral("name"), sanitize(device->name())},
            {QStringLiteral("vendor"), sanitize(device->vendor())},
            {QStringLiteral("type"), device->typeName()},
            {QStringLiteral("backendId"), sanitize(device->backendId())},
            {QStringLiteral("discoveryIdentity"), sanitize(device->discoveryIdentity())},
            {QStringLiteral("discoverySupportStage"), sanitize(device->discoverySupportStage())},
            {QStringLiteral("discoverySupportStatus"), sanitize(device->discoverySupportStatus())},
            {QStringLiteral("discoverySupportFamily"), sanitize(device->discoverySupportFamily())},
            {QStringLiteral("discoverySupportNotes"), sanitize(device->discoverySupportNotes())},
            {QStringLiteral("discoveryCataloged"), device->discoveryCataloged()},
            {QStringLiteral("discoveryWriteCapableBackend"), device->discoveryWriteCapableBackend()},
            {QStringLiteral("likelyRgbController"), device->likelyRgbController()},
            {QStringLiteral("hasRgbControllerOverride"), device->hasRgbControllerOverride()},
            {QStringLiteral("isRgbController"), device->isRgbController()},
            {QStringLiteral("requiresConfirmation"), requiresConfirmation},
            {QStringLiteral("writeConfirmed"), writeConfirmed},
            {QStringLiteral("writable"), writable},
            {QStringLiteral("lastHardwareWriteStatus"), sanitize(device->lastHardwareWriteStatus())},
            {QStringLiteral("capabilities"), deviceCapabilities},
            {QStringLiteral("permissions"), permissions},
            {QStringLiteral("supportedEffects"), effects},
            {QStringLiteral("zones"), zones},
        });
    }

    QVariantList activity;
    for (const QString& line : m_logLines) {
        activity.append(sanitize(line));
    }

    const QString generatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QString applicationFilePath = QCoreApplication::applicationFilePath();
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QString bundledDaemonPath =
        QDir(applicationDirectory).filePath(bundledDaemonExecutableName());
    const QVariantMap counts {
        {QStringLiteral("devices"), devices.size()},
        {QStringLiteral("zones"), totalZoneCount},
        {QStringLiteral("profiles"), profileNames.size()},
        {QStringLiteral("activityLines"), activity.size()},
        {QStringLiteral("writableDevices"), writableDeviceCount},
        {QStringLiteral("readOnlyDevices"), readOnlyDeviceCount},
        {QStringLiteral("confirmationRequiredDevices"), confirmationRequiredDeviceCount},
        {QStringLiteral("confirmedDevices"), confirmedDeviceCount},
        {QStringLiteral("pendingDaemonOperations"), pendingDaemonOperations()},
    };

    return QVariantMap {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("generatedAt"), generatedAt},
        {
            QStringLiteral("summary"),
            QVariantMap {
                {QStringLiteral("generatedAt"), generatedAt},
                {QStringLiteral("setup"), setupStatusSummary()},
                {QStringLiteral("setupLevel"), setupStatusLevel()},
                {QStringLiteral("backend"), backend.displayName},
                {QStringLiteral("backendId"), backend.id},
                {QStringLiteral("backendEffectiveId"), effectiveBackendId},
                {QStringLiteral("deviceCount"), devices.size()},
                {QStringLiteral("zoneCount"), totalZoneCount},
                {QStringLiteral("writableDeviceCount"), writableDeviceCount},
                {QStringLiteral("confirmationRequiredDeviceCount"), confirmationRequiredDeviceCount},
                {QStringLiteral("dryRunEnabled"), dryRunEnabled()},
                {QStringLiteral("daemonDryRunKnown"), daemonDryRunKnown()},
                {QStringLiteral("daemonDryRunEnabled"), daemonDryRunEnabled()},
                {QStringLiteral("dryRunSynchronized"), !daemonDryRunMismatch()},
                {QStringLiteral("daemonState"), daemonState()},
                {QStringLiteral("daemonConnected"), daemonConnected()},
            },
        },
        {
            QStringLiteral("diagnosticScope"),
            QVariantMap {
                {QStringLiteral("profileContentsIncluded"), false},
                {QStringLiteral("profileNamesIncluded"), true},
                {QStringLiteral("activityLinesIncluded"), true},
                {QStringLiteral("activityLineLimit"), m_deviceManager->activityLog().maxLineCount()},
                {QStringLiteral("pathRedaction"), QStringLiteral("<home>, <temp>, <app-data>, and <profiles> paths are redacted where known. <app-data> refers to LumaCore's active application data root.")},
            },
        },
        {
            QStringLiteral("application"),
            QVariantMap {
                {QStringLiteral("name"), QStringLiteral("LumaCore")},
                {QStringLiteral("version"), QStringLiteral(LUMACORE_VERSION)},
                {QStringLiteral("qtVersion"), QString::fromUtf8(qVersion())},
                {QStringLiteral("platform"), QSysInfo::prettyProductName()},
                {QStringLiteral("buildAbi"), QSysInfo::buildAbi()},
                {QStringLiteral("kernel"), QSysInfo::kernelType()},
                {QStringLiteral("kernelVersion"), QSysInfo::kernelVersion()},
                {QStringLiteral("executablePath"), sanitize(applicationFilePath)},
                {QStringLiteral("applicationDirectory"), sanitize(applicationDirectory)},
            },
        },
        {
            QStringLiteral("environment"),
            QVariantMap {
                {QStringLiteral("qtVersion"), QString::fromUtf8(qVersion())},
                {QStringLiteral("productType"), QSysInfo::productType()},
                {QStringLiteral("productVersion"), QSysInfo::productVersion()},
                {QStringLiteral("currentCpuArchitecture"), QSysInfo::currentCpuArchitecture()},
                {QStringLiteral("buildCpuArchitecture"), QSysInfo::buildCpuArchitecture()},
                {QStringLiteral("buildAbi"), QSysInfo::buildAbi()},
                {QStringLiteral("kernel"), QSysInfo::kernelType()},
                {QStringLiteral("kernelVersion"), QSysInfo::kernelVersion()},
            },
        },
        {
            QStringLiteral("storage"),
            QVariantMap {
                {QStringLiteral("dataRoot"), sanitize(portableDataRoot())},
                {QStringLiteral("settingsDirectory"), sanitize(portableSettingsDirectory())},
                {QStringLiteral("profilesDirectory"), sanitize(profilesDirectory)},
                {QStringLiteral("cacheDirectory"), sanitize(portableCacheDirectory())},
            },
        },
        {
            QStringLiteral("package"),
            QVariantMap {
                {QStringLiteral("applicationDirectory"), sanitize(applicationDirectory)},
                {QStringLiteral("expectedBundledDaemon"), sanitize(bundledDaemonPath)},
                {QStringLiteral("bundledDaemonPresent"), QFileInfo::exists(bundledDaemonPath)},
            },
        },
        {QStringLiteral("counts"), counts},
        {
            QStringLiteral("backend"),
            QVariantMap {
                {QStringLiteral("id"), backend.id},
                {QStringLiteral("effectiveId"), effectiveBackendId},
                {QStringLiteral("displayName"), backend.displayName},
                {QStringLiteral("description"), backend.description},
                {QStringLiteral("capabilities"), backendCapabilities},
                {QStringLiteral("deviceCount"), m_deviceManager->deviceCount()},
                {QStringLiteral("availableBackendIds"), m_deviceManager->backendRegistry().availableBackendIds()},
            },
        },
        {
            QStringLiteral("setup"),
            QVariantMap {
                {QStringLiteral("level"), setupStatusLevel()},
                {QStringLiteral("summary"), setupStatusSummary()},
                {QStringLiteral("detail"), sanitize(setupStatusDetail())},
                {QStringLiteral("action"), sanitize(setupStatusAction())},
                {QStringLiteral("attentionRequired"), setupAttentionRequired()},
            },
        },
        {
            QStringLiteral("daemon"),
            QVariantMap {
                {QStringLiteral("state"), daemonState()},
                {QStringLiteral("connected"), daemonConnected()},
                {QStringLiteral("recoveryBusy"), daemonRecoveryBusy()},
                {QStringLiteral("socketPath"), sanitize(daemonSocketPath())},
                {QStringLiteral("version"), daemonVersion()},
                {QStringLiteral("lastError"), sanitize(daemonLastError())},
                {QStringLiteral("pendingOperations"), pendingDaemonOperations()},
            },
        },
        {
            QStringLiteral("safety"),
            QVariantMap {
                {QStringLiteral("dryRunEnabled"), dryRunEnabled()},
                {QStringLiteral("daemonDryRunKnown"), daemonDryRunKnown()},
                {QStringLiteral("daemonDryRunEnabled"), daemonDryRunEnabled()},
                {QStringLiteral("dryRunSynchronized"), !daemonDryRunMismatch()},
                {QStringLiteral("profilesDirectory"), sanitize(profilesDirectory)},
            },
        },
        {QStringLiteral("profiles"), profileNames},
        {QStringLiteral("devices"), devices},
        {QStringLiteral("activity"), activity},
    };
}

QString AppController::diagnosticsSummaryText() const
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    return diagnosticsSummaryTextFor(diagnosticsReport());
}

bool AppController::copyDiagnosticsSummary()
{
    if (m_deviceManager == nullptr) {
        setStatusMessage(QStringLiteral("Diagnostics summary is unavailable."));
        return false;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        setStatusMessage(QStringLiteral("Clipboard is unavailable."));
        return false;
    }

    clipboard->setText(diagnosticsSummaryText());
    setStatusMessage(QStringLiteral("Copied diagnostics summary."));
    return true;
}

bool AppController::exportDiagnostics(const QUrl& destinationUrl)
{
    if (m_deviceManager == nullptr || !destinationUrl.isLocalFile()) {
        setStatusMessage(QStringLiteral("Choose a local destination for the diagnostics export."));
        return false;
    }

    QString destinationPath = destinationUrl.toLocalFile();
    if (QFileInfo(destinationPath).suffix().isEmpty()) {
        destinationPath.append(QStringLiteral(".json"));
    }

    QSaveFile output(destinationPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatusMessage(QStringLiteral("Could not open diagnostics export destination."));
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromVariant(diagnosticsReport());
    output.write(document.toJson(QJsonDocument::Indented));
    if (!output.commit()) {
        setStatusMessage(QStringLiteral("Could not write diagnostics export."));
        return false;
    }

    setStatusMessage(QStringLiteral("Exported diagnostics report."));
    return true;
}

QVariantMap AppController::profileCompatibility(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return {};
    }

    const QVariantMap report = m_deviceManager->profileCompatibility(profileName);
    if (!report.value(QStringLiteral("valid")).toBool()) {
        setStatusMessage(report.value(QStringLiteral("error")).toString());
    }
    return report;
}

bool AppController::profileExists(const QString& profileName) const
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    return m_deviceManager->profileNames().contains(ProfileStore::normalizeName(profileName));
}

QStringList AppController::profileNames() const
{
    return m_deviceManager == nullptr ? QStringList {} : m_deviceManager->profileNames();
}

bool AppController::retryDaemonConnection()
{
    if (m_daemonClient == nullptr) {
        setStatusMessage(QStringLiteral("Daemon client is not available."));
        return false;
    }
    if (m_daemonClient->isConnected()) {
        return rescanDaemonDevices();
    }

    m_daemonClient->setAutomaticReconnectEnabled(true);
    m_daemonClient->reconnectNow();
    emit daemonInfoChanged();
    emit setupStatusChanged();
    return true;
}

bool AppController::rescanDaemonDevices()
{
    if (!daemonConnected()) {
        setStatusMessage(QStringLiteral("Cannot rescan devices while the daemon is offline."));
        return false;
    }
    return refreshDaemonDevices(false);
}

bool AppController::applyProfileOnLaunch(const QString& profileName)
{
    const QString selectedProfile = profileName.trimmed();
    if (selectedProfile.isEmpty()) {
        setStatusMessage(QStringLiteral("No active profile is selected for launch."));
        return false;
    }

    if (!loadProfile(selectedProfile)) {
        return false;
    }

    setStatusMessage(QStringLiteral("Applied active profile '%1' on launch.").arg(selectedProfile));
    return true;
}

bool AppController::applyScheduledProfile(const QString& profileName)
{
    const QString selectedProfile = profileName.trimmed();
    if (selectedProfile.isEmpty()) {
        setStatusMessage(QStringLiteral("No profile is selected for the schedule."));
        return false;
    }

    if (!loadProfile(selectedProfile)) {
        return false;
    }

    setStatusMessage(QStringLiteral("Applied scheduled profile '%1'.").arg(selectedProfile));
    return true;
}

void AppController::enableDaemonRecovery()
{
    if (m_daemonClient == nullptr || m_daemonRecoveryEnabled) {
        return;
    }

    m_daemonRecoveryEnabled = true;
    m_daemonClient->setAutomaticReconnectEnabled(true);
}

void AppController::appendLog(const QString& message)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    m_logLines.append(message);
    while (m_logLines.size() > 500) {
        m_logLines.removeFirst();
    }
    emit logTextChanged();
}

QString AppController::normalizeDeviceGroupName(const QString& groupName)
{
    return groupName.simplified();
}

QStringList AppController::storedDeviceGroupIds(const QString& groupName) const
{
    const QString normalizedName = normalizeDeviceGroupName(groupName);
    return DeviceGroupStore().deviceIds(normalizedName);
}

void AppController::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message) {
        return;
    }

    m_statusMessage = message;
    emit statusMessageChanged();
}

void AppController::setLocalDaemonWriteConfirmed(int deviceIndex, bool confirmed)
{
    RgbDevice* device = deviceAt(deviceIndex);
    auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(device);
    if (daemonDevice == nullptr) {
        return;
    }

    daemonDevice->setWriteConfirmed(confirmed);
    emit writeConfirmationChanged(deviceIndex);
    emit setupStatusChanged();
    emit zoneDataChanged(deviceIndex, -1);
}

RgbDevice* AppController::deviceAt(int deviceIndex)
{
    return m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
}

const RgbDevice* AppController::deviceAt(int deviceIndex) const
{
    return m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
}

const RgbZone* AppController::zoneAt(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return nullptr;
    }
    return &device->zones().at(zoneIndex);
}

void AppController::refreshBackendInfo()
{
    emit backendInfoChanged();
    emit daemonInfoChanged();
    emit setupStatusChanged();
}

void AppController::refreshDaemonActivityLog()
{
    if (m_daemonClient == nullptr || !m_daemonClient->isConnected()) {
        return;
    }

    const QPointer<AppController> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::ActivityLogSnapshot),
        {},
        [self](DaemonCallResult response) {
            if (self == nullptr || !response.ok) {
                return;
            }
            const QJsonArray lines = response.result.value(QStringLiteral("lines")).toArray();
            for (const QJsonValue& line : lines) {
                const QString text = line.toString();
                if (!text.isEmpty() && !self->m_logLines.contains(text)) {
                    self->appendLog(text);
                }
            }
        },
        1000
    );
    Q_UNUSED(requestId)
}

void AppController::beginDaemonOperation()
{
    ++m_pendingDaemonOperations;
    emit pendingDaemonOperationsChanged();
}

void AppController::endDaemonOperation()
{
    if (m_pendingDaemonOperations <= 0) {
        return;
    }
    --m_pendingDaemonOperations;
    emit pendingDaemonOperationsChanged();
}

bool AppController::refreshDaemonDevices(bool recoveredConnection)
{
    if (m_daemonClient == nullptr || m_deviceManager == nullptr || m_daemonRefreshInProgress) {
        return false;
    }

    auto* daemonBackend =
        dynamic_cast<DaemonBackend*>(m_deviceManager->backendRegistry().activeBackend());
    if (daemonBackend == nullptr) {
        return false;
    }

    m_daemonRefreshInProgress = true;
    emit daemonInfoChanged();
    emit setupStatusChanged();
    setStatusMessage(
        recoveredConnection
            ? QStringLiteral("Reconnected to daemon. Refreshing devices.")
            : QStringLiteral("Rescanning daemon devices.")
    );
    const QPointer<AppController> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::ListDevices),
        {},
        [self, daemonBackend, recoveredConnection](DaemonCallResult response) {
            if (self == nullptr) {
                return;
            }
            self->m_daemonRefreshInProgress = false;
            emit self->daemonInfoChanged();
            emit self->setupStatusChanged();
            if (!response.ok) {
                self->setStatusMessage(
                    response.error.isEmpty()
                        ? (recoveredConnection
                            ? QStringLiteral("Reconnected, but device refresh failed.")
                            : QStringLiteral("Device rescan failed."))
                        : response.error
                );
                return;
            }

            self->m_deviceManager->replaceDevices(daemonBackend->devicesFromPayload(response.result));
            self->m_daemonDevicesLoaded = true;
            self->setStatusMessage(
                recoveredConnection
                    ? QStringLiteral("Reconnected and refreshed %1 device(s).")
                          .arg(self->m_deviceManager->deviceCount())
                    : QStringLiteral("Rescan loaded %1 device(s).")
                          .arg(self->m_deviceManager->deviceCount())
            );
            self->syncDaemonDryRun();
            self->refreshBackendInfo();
            emit self->daemonDevicesRefreshed();
        },
        3000
    );
    Q_UNUSED(requestId)
    return true;
}

void AppController::syncDaemonDryRun()
{
    if (m_daemonClient == nullptr || !m_daemonClient->isConnected()) {
        m_pendingDaemonDryRunSync = false;
        return;
    }
    if (m_daemonDryRunSyncInProgress) {
        m_pendingDaemonDryRunSync = true;
        return;
    }

    const bool requestedDryRun = dryRunEnabled();
    m_daemonDryRunSyncInProgress = true;
    m_pendingDaemonDryRunSync = false;
    beginDaemonOperation();
    const QPointer<AppController> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::SetDryRun),
        {{QStringLiteral("enabled"), requestedDryRun}},
        [self, requestedDryRun](DaemonCallResult response) {
            if (self == nullptr) {
                return;
            }

            self->m_daemonDryRunSyncInProgress = false;
            self->endDaemonOperation();
            const bool success = response.ok
                && response.result.value(QStringLiteral("success")).toBool(false)
                && response.result.value(QStringLiteral("dryRunEnabled")).toBool(!requestedDryRun) == requestedDryRun;
            if (success) {
                emit self->daemonInfoChanged();
                self->refreshDaemonActivityLog();
            } else {
                self->setStatusMessage(
                    response.ok
                        ? QStringLiteral("Could not synchronize daemon dry-run mode.")
                        : response.error
                );
            }

            if (self->m_pendingDaemonDryRunSync || self->dryRunEnabled() != requestedDryRun) {
                self->syncDaemonDryRun();
            }
        }
    );
    Q_UNUSED(requestId)
}

} // namespace lumacore
