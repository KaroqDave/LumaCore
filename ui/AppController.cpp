#include "ui/AppController.h"

#include "backends/daemon/DaemonRgbDevice.h"
#include "core/ActivityLog.h"
#include "core/RgbBackend.h"
#include "core/RgbColor.h"
#include "core/RgbEffect.h"

#include <QJsonArray>
#include <QtGlobal>

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
    return m_daemonClient != nullptr && m_daemonClient->isConnected();
}

QString AppController::daemonState() const
{
    return daemonConnected() ? QStringLiteral("Connected") : QStringLiteral("Disconnected");
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

bool AppController::dryRunEnabled() const
{
    return m_deviceManager != nullptr && m_deviceManager->dryRunEnabled();
}

void AppController::setDryRunEnabled(bool enabled)
{
    if (m_deviceManager == nullptr) {
        return;
    }

    m_deviceManager->setDryRunEnabled(enabled);
    syncDaemonDryRun();
}

bool AppController::applyStaticColor(int deviceIndex, int zoneIndex, const QColor& color)
{
    return applyEffect(deviceIndex, zoneIndex, static_cast<int>(RgbEffectType::Static), color, 1.0, 100);
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
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return static_cast<int>(RgbEffectType::Static);
    }

    return static_cast<int>(device->zones().at(zoneIndex).effect().type());
}

QColor AppController::zoneEffectColor(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).effect().color().toQColor();
}

double AppController::zoneEffectSpeed(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return 1.0;
    }

    return device->zones().at(zoneIndex).effect().speed();
}

int AppController::zoneEffectBrightness(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return 100;
    }

    return device->zones().at(zoneIndex).effect().brightness();
}

bool AppController::updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount)
{
    if (m_deviceManager == nullptr) {
        return false;
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
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device == nullptr ? 0 : static_cast<int>(device->zones().size());
}

QString AppController::deviceName(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device == nullptr ? QString() : device->name();
}

bool AppController::deviceWritable(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    const BackendCapabilities capabilities = device->capabilities();
    return capabilities.testFlag(BackendCapability::ZoneColorWrite)
        || capabilities.testFlag(BackendCapability::ZoneEffectWrite);
}

QString AppController::devicePermissionReason(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
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
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device == nullptr ? QString() : device->lastHardwareWriteStatus();
}

bool AppController::deviceSupportsEffect(int deviceIndex, int effectType) const
{
    if (!isValidEffectType(effectType)) {
        return false;
    }

    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    if (const auto* daemonDevice = dynamic_cast<const DaemonRgbDevice*>(device)) {
        return daemonDevice->supportsEffect(effectType);
    }

    const BackendCapabilities capabilities = device->capabilities();
    if (!isAnimatedEffect(effectType)) {
        return capabilities.testFlag(BackendCapability::ZoneColorWrite);
    }

    return capabilities.testFlag(BackendCapability::ZoneEffectWrite);
}

bool AppController::deviceSupportsEffectSpeed(int deviceIndex, int effectType) const
{
    if (!deviceSupportsEffect(deviceIndex, effectType) || !isAnimatedEffect(effectType)) {
        return false;
    }

    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    if (const auto* daemonDevice = dynamic_cast<const DaemonRgbDevice*>(device)) {
        return daemonDevice->supportsEffectSpeed(effectType);
    }

    return true;
}

bool AppController::deviceSupportsEffectBrightness(int deviceIndex, int effectType) const
{
    if (!deviceSupportsEffect(deviceIndex, effectType)) {
        return false;
    }

    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    if (const auto* daemonDevice = dynamic_cast<const DaemonRgbDevice*>(device)) {
        return daemonDevice->supportsEffectBrightness(effectType);
    }

    return true;
}

bool AppController::deviceRequiresConfirmation(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    return device->checkRuntimePermission(BackendCapability::ZoneColorWrite).status == PermissionStatus::RequiresConfirmation;
}

bool AppController::deviceWriteConfirmed(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
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

    const DaemonCallResult response = m_daemonClient->call(QStringLiteral("confirmWrites"), {
        {QStringLiteral("deviceIndex"), deviceIndex},
    });
    if (!response.ok || !response.result.value(QStringLiteral("success")).toBool(false)) {
        setStatusMessage(response.ok ? QStringLiteral("Could not confirm hardware writes.") : response.error);
        refreshDaemonActivityLog();
        return false;
    }

    setStatusMessage(QStringLiteral("Hardware writes confirmed for this daemon session."));
    setLocalDaemonWriteConfirmed(deviceIndex, response.result.value(QStringLiteral("writeConfirmed")).toBool(true));
    refreshDaemonActivityLog();
    emit backendInfoChanged();
    return true;
}

bool AppController::revokeDeviceWrites(int deviceIndex)
{
    if (m_daemonClient == nullptr || !m_daemonClient->isConnected()) {
        setStatusMessage(QStringLiteral("Could not revoke hardware writes: daemon is not connected."));
        return false;
    }

    const DaemonCallResult response = m_daemonClient->call(QStringLiteral("revokeWrites"), {
        {QStringLiteral("deviceIndex"), deviceIndex},
    });
    if (!response.ok || !response.result.value(QStringLiteral("success")).toBool(false)) {
        setStatusMessage(response.ok ? QStringLiteral("Could not revoke hardware writes.") : response.error);
        refreshDaemonActivityLog();
        return false;
    }

    setStatusMessage(QStringLiteral("Hardware write confirmation revoked."));
    setLocalDaemonWriteConfirmed(deviceIndex, response.result.value(QStringLiteral("writeConfirmed")).toBool(false));
    refreshDaemonActivityLog();
    emit backendInfoChanged();
    return true;
}

bool AppController::allOffDevice(int deviceIndex)
{
    if (m_deviceManager == nullptr) {
        setStatusMessage(QStringLiteral("Could not turn device off."));
        return false;
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

bool AppController::deviceIsRgbController(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device != nullptr && device->isRgbController();
}

bool AppController::deviceHasRgbControllerOverride(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device != nullptr && device->hasRgbControllerOverride();
}

bool AppController::deviceRgbControllerOverride(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device != nullptr && device->rgbControllerOverride();
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

QString AppController::zoneName(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).name();
}

QString AppController::zoneTypeName(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).typeName();
}

int AppController::zoneLedCount(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return 0;
    }

    return device->zones().at(zoneIndex).ledCount();
}

QColor AppController::zoneColor(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).currentColor().toQColor();
}

QString AppController::zoneColorHex(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).currentColor().toHexString();
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
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool loaded = m_deviceManager->loadProfile(profileName, &errorMessage);
    if (!loaded && !errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
    } else if (loaded) {
        refreshDaemonActivityLog();
    }

    return loaded;
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

QStringList AppController::profileNames() const
{
    return m_deviceManager == nullptr ? QStringList {} : m_deviceManager->profileNames();
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
    RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    auto* daemonDevice = dynamic_cast<DaemonRgbDevice*>(device);
    if (daemonDevice == nullptr) {
        return;
    }

    daemonDevice->setWriteConfirmed(confirmed);
    emit writeConfirmationChanged(deviceIndex);
    emit zoneDataChanged(deviceIndex, -1);
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

    const DaemonCallResult response = m_daemonClient->call(QStringLiteral("activityLogSnapshot"), {}, 1000);
    if (!response.ok) {
        return;
    }

    const QJsonArray lines = response.result.value(QStringLiteral("lines")).toArray();
    for (const QJsonValue& line : lines) {
        const QString text = line.toString();
        if (!text.isEmpty() && !m_logLines.contains(text)) {
            appendLog(text);
        }
    }
}

void AppController::syncDaemonDryRun()
{
    if (m_daemonClient == nullptr || !m_daemonClient->isConnected()) {
        return;
    }

    const DaemonCallResult response = m_daemonClient->call(QStringLiteral("setDryRun"), {
        {QStringLiteral("enabled"), dryRunEnabled()},
    });
    if (response.ok) {
        emit daemonInfoChanged();
        refreshDaemonActivityLog();
    }
}

} // namespace lumacore
