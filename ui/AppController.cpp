#include "ui/AppController.h"

#include "backends/daemon/DaemonBackend.h"
#include "backends/daemon/DaemonRgbDevice.h"
#include "core/ActivityLog.h"
#include "core/RgbBackend.h"
#include "core/RgbColor.h"
#include "core/RgbEffect.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
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
    return effectType >= static_cast<int>(RgbEffectType::Static)
        && effectType <= static_cast<int>(RgbEffectType::ColorCycle);
}

bool isAnimatedEffect(int effectType)
{
    return effectType != static_cast<int>(RgbEffectType::Static);
}

QString sanitizedDiagnosticString(QString value, const QString& profilesDirectory = {})
{
    const QString homePath = QDir::toNativeSeparators(QDir::homePath());
    const QString tempPath = QDir::toNativeSeparators(QDir::tempPath());
    const QString appDataPath = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    );
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

QVariantMap permissionToDiagnostics(const PermissionResult& permission)
{
    return QVariantMap {
        {QStringLiteral("status"), permissionStatusDiagnosticName(permission.status)},
        {QStringLiteral("reason"), permission.reason},
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
    int total {0};
    int applied {0};
    int skipped {0};
    int failed {0};
    int pending {0};
    bool dispatching {true};
    QStringList details;
};

} // namespace

AppController::AppController(DeviceManager* deviceManager, std::shared_ptr<DaemonClient> daemonClient, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
    , m_daemonClient(std::move(daemonClient))
{
    setStatusMessage(QStringLiteral("Connecting to LumaCore daemon."));

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
    });

    connect(m_deviceManager, &DeviceManager::dryRunEnabledChanged, this, [this]() {
        emit dryRunEnabledChanged();
    });

    if (m_daemonClient != nullptr) {
        connect(m_daemonClient.get(), &DaemonClient::connectionStateChanged, this, [this]() {
            emit daemonInfoChanged();
            if (daemonConnected()) {
                syncDaemonDryRun();
            }
            setStatusMessage(daemonConnected() ? QStringLiteral("Connected to LumaCore daemon.") : QStringLiteral("Daemon disconnected."));
        });
        connect(m_daemonClient.get(), &DaemonClient::lastErrorChanged, this, [this]() {
            emit daemonInfoChanged();
            if (!daemonLastError().isEmpty()) {
                setStatusMessage(daemonLastError());
            }
        });
        connect(m_daemonClient.get(), &DaemonClient::daemonInfoChanged, this, [this]() {
            emit daemonInfoChanged();
        });
        connect(m_daemonClient.get(), &DaemonClient::reconnected, this, [this]() {
            if (m_daemonRecoveryEnabled) {
                QTimer::singleShot(0, this, [this] { refreshDaemonDevices(true); });
            }
        });
        connect(m_daemonClient.get(), &DaemonClient::reconnectScheduled, this, [this](int attempt, int delayMs) {
            emit daemonInfoChanged();
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

bool AppController::dryRunEnabled() const
{
    return m_deviceManager != nullptr && m_deviceManager->dryRunEnabled();
}

int AppController::pendingDaemonOperations() const
{
    return m_pendingDaemonOperations;
}

void AppController::setDryRunEnabled(bool enabled)
{
    if (m_deviceManager == nullptr) {
        return;
    }

    m_deviceManager->setDryRunEnabled(enabled);
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

    if (!deviceSupportsEffect(deviceIndex, effectType)) {
        setStatusMessage(QStringLiteral("Selected device does not support this effect."));
        return false;
    }

    const RgbEffect effect(
        static_cast<RgbEffectType>(effectType),
        RgbColor::fromQColor(color),
        speed,
        brightness
    );

    if (auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(deviceAt(deviceIndex))) {
        beginDaemonOperation();
        setStatusMessage(QStringLiteral("Applying effect to selected zone."));
        const QPointer<AppController> self(this);
        const quint64 requestId = daemonDevice->applyZoneEffectAsync(
            zoneIndex,
            effect,
            !dryRunEnabled(),
            [self, deviceIndex, zoneIndex](bool success, const QString& error) {
                if (self == nullptr) {
                    return;
                }
                self->endDaemonOperation();
                self->setStatusMessage(
                    success
                        ? QStringLiteral("Applied effect to selected zone.")
                        : (error.isEmpty() ? QStringLiteral("Could not apply effect to selected zone.") : error)
                );
                emit self->zoneDataChanged(deviceIndex, zoneIndex);
                self->refreshDaemonActivityLog();
            }
        );
        if (requestId == 0) {
            endDaemonOperation();
            setStatusMessage(QStringLiteral("Could not queue effect for selected zone."));
            return false;
        }
        return true;
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
        if (requestId == 0) {
            endDaemonOperation();
            setStatusMessage(QStringLiteral("Could not queue zone update."));
            return false;
        }
        return true;
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

    const BackendCapabilities capabilities = device->capabilities();
    return capabilities.testFlag(BackendCapability::ZoneColorWrite)
        || capabilities.testFlag(BackendCapability::ZoneEffectWrite);
}

QString AppController::devicePermissionReason(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return {};
    }

    const PermissionResult colorPermission = device->checkRuntimePermission(BackendCapability::ZoneColorWrite);
    if (!colorPermission.reason.isEmpty()) {
        return colorPermission.reason;
    }

    const PermissionResult effectPermission = device->checkRuntimePermission(BackendCapability::ZoneEffectWrite);
    return effectPermission.reason;
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

bool AppController::deviceRequiresConfirmation(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return device->checkRuntimePermission(BackendCapability::ZoneColorWrite).status == PermissionStatus::RequiresConfirmation;
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
        const quint64 requestId = daemonDevice->applyAllOffAsync(
            !dryRunEnabled(),
            [self, deviceIndex](bool success, const QString& error) {
                if (self == nullptr) {
                    return;
                }
                self->endDaemonOperation();
                self->setStatusMessage(
                    success
                        ? QStringLiteral("Sent all-off command to selected device.")
                        : (error.isEmpty() ? QStringLiteral("Could not turn device off.") : error)
                );
                emit self->zoneDataChanged(deviceIndex, -1);
                self->refreshDaemonActivityLog();
            }
        );
        if (requestId == 0) {
            endDaemonOperation();
            setStatusMessage(QStringLiteral("Could not queue all-off command."));
            return false;
        }
        return true;
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
    return applyGlobalEffectInternal(effectType, color, speed, brightness, false);
}

bool AppController::setGlobalBrightness(int brightness)
{
    return applyGlobalEffectInternal(
        static_cast<int>(RgbEffectType::Static),
        QColor(Qt::black),
        1.0,
        brightness,
        true
    );
}

bool AppController::applyGlobalEffectInternal(
    int effectType,
    const QColor& color,
    double speed,
    int brightness,
    bool preserveCurrentEffect
)
{
    if (m_deviceManager == nullptr
        || (!preserveCurrentEffect && (!isValidEffectType(effectType) || !color.isValid()))) {
        setStatusMessage(QStringLiteral("Could not start global lighting operation."));
        return false;
    }

    auto state = std::make_shared<GlobalOperationState>();
    state->operation = preserveCurrentEffect
        ? QStringLiteral("Global brightness")
        : QStringLiteral("Global effect");
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

            if (!deviceSupportsEffect(deviceIndex, static_cast<int>(effect.type()))) {
                ++state->skipped;
                state->details.append(
                    QStringLiteral("%1 / %2: effect is not supported.")
                        .arg(device->name(), device->zones().at(zoneIndex).name())
                );
                continue;
            }
            if (preserveCurrentEffect
                && !deviceSupportsEffectBrightness(deviceIndex, static_cast<int>(effect.type()))) {
                ++state->skipped;
                state->details.append(
                    QStringLiteral("%1 / %2: brightness control is not supported.")
                        .arg(device->name(), device->zones().at(zoneIndex).name())
                );
                continue;
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
                const quint64 requestId = daemonDevice->applyZoneEffectAsync(
                    zoneIndex,
                    effect,
                    !dryRunEnabled(),
                    [self, state, finish, deviceIndex, zoneIndex](bool success, const QString& error) {
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
                        emit self->zoneDataChanged(deviceIndex, zoneIndex);
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
        setStatusMessage(QStringLiteral("No writable zones are available."));
        return false;
    }

    setStatusMessage(QStringLiteral("%1 started for %2 target(s).").arg(state->operation).arg(state->total));
    (*finish)();
    return true;
}

bool AppController::allOffAllDevices()
{
    if (m_deviceManager == nullptr) {
        setStatusMessage(QStringLiteral("Could not start global All Off."));
        return false;
    }

    auto state = std::make_shared<GlobalOperationState>();
    state->operation = QStringLiteral("All Off");
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
            const quint64 requestId = daemonDevice->applyAllOffAsync(
                !dryRunEnabled(),
                [self, state, finish, deviceIndex](bool success, const QString& error) {
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
                    emit self->zoneDataChanged(deviceIndex, -1);
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
        setStatusMessage(QStringLiteral("No writable devices are available."));
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

    QVariantList devices;
    for (int deviceIndex = 0; deviceIndex < m_deviceManager->deviceCount(); ++deviceIndex) {
        const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
        if (device == nullptr) {
            continue;
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
                permissionToDiagnostics(device->checkRuntimePermission(BackendCapability::DiscoveryRead)),
            },
            {
                backendCapabilityToString(BackendCapability::ZoneColorWrite),
                permissionToDiagnostics(device->checkRuntimePermission(BackendCapability::ZoneColorWrite)),
            },
            {
                backendCapabilityToString(BackendCapability::ZoneEffectWrite),
                permissionToDiagnostics(device->checkRuntimePermission(BackendCapability::ZoneEffectWrite)),
            },
        };

        QVariantList effects;
        for (const RgbEffectType effectType : {
                 RgbEffectType::Static,
                 RgbEffectType::Rainbow,
                 RgbEffectType::Breathing,
                 RgbEffectType::ColorCycle,
             }) {
            const int effectTypeValue = static_cast<int>(effectType);
            effects.append(QVariantMap {
                {QStringLiteral("type"), rgbEffectTypeToString(effectType)},
                {QStringLiteral("supported"), device->supportsEffect(effectTypeValue)},
                {QStringLiteral("speed"), device->supportsEffectSpeed(effectTypeValue)},
                {QStringLiteral("brightness"), device->supportsEffectBrightness(effectTypeValue)},
            });
        }

        QVariantList zones;
        const QVector<RgbZone>& deviceZones = device->zones();
        for (int zoneIndex = 0; zoneIndex < deviceZones.size(); ++zoneIndex) {
            const RgbZone& zone = deviceZones.at(zoneIndex);
            zones.append(QVariantMap {
                {QStringLiteral("index"), zoneIndex},
                {QStringLiteral("name"), zone.name()},
                {QStringLiteral("type"), zone.typeName()},
                {QStringLiteral("ledCount"), zone.ledCount()},
                {QStringLiteral("currentColor"), zone.currentColor().toHexString()},
                {QStringLiteral("effect"), effectToDiagnostics(zone.effect())},
            });
        }

        devices.append(QVariantMap {
            {QStringLiteral("index"), deviceIndex},
            {QStringLiteral("id"), device->id()},
            {QStringLiteral("name"), device->name()},
            {QStringLiteral("vendor"), device->vendor()},
            {QStringLiteral("type"), device->typeName()},
            {QStringLiteral("backendId"), device->backendId()},
            {QStringLiteral("discoveryIdentity"), device->discoveryIdentity()},
            {QStringLiteral("likelyRgbController"), device->likelyRgbController()},
            {QStringLiteral("hasRgbControllerOverride"), device->hasRgbControllerOverride()},
            {QStringLiteral("isRgbController"), device->isRgbController()},
            {QStringLiteral("writeConfirmed"), m_deviceManager->deviceWriteConfirmed(deviceIndex)},
            {QStringLiteral("writable"), deviceWritable(deviceIndex)},
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

    return QVariantMap {
        {QStringLiteral("schemaVersion"), 1},
        {
            QStringLiteral("generatedAt"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
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
            },
        },
        {
            QStringLiteral("backend"),
            QVariantMap {
                {QStringLiteral("id"), backend.id},
                {QStringLiteral("displayName"), backend.displayName},
                {QStringLiteral("description"), backend.description},
                {QStringLiteral("capabilities"), backendCapabilities},
                {QStringLiteral("deviceCount"), m_deviceManager->deviceCount()},
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
                {QStringLiteral("profilesDirectory"), sanitize(profilesDirectory)},
            },
        },
        {QStringLiteral("profiles"), profileNames},
        {QStringLiteral("devices"), devices},
        {QStringLiteral("activity"), activity},
    };
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
        return;
    }

    const QPointer<AppController> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::SetDryRun),
        {{QStringLiteral("enabled"), dryRunEnabled()}},
        [self](DaemonCallResult response) {
            if (self != nullptr && response.ok) {
                emit self->daemonInfoChanged();
                self->refreshDaemonActivityLog();
            }
        }
    );
    Q_UNUSED(requestId)
}

} // namespace lumacore
