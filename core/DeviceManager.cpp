#include "core/DeviceManager.h"

#include "core/PermissionGate.h"
#include "core/WriteGate.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>

#include <utility>

namespace lumacore {

namespace {

int storedEffectType(const QJsonObject& zoneObject)
{
    const QJsonObject effectObject = zoneObject.value(QStringLiteral("effect")).toObject();
    if (effectObject.isEmpty()) {
        return static_cast<int>(RgbEffectType::Static);
    }

    const QString effectName = effectObject.value(QStringLiteral("type")).toString();
    const QStringList knownEffects {
        QStringLiteral("Static"),
        QStringLiteral("Rainbow"),
        QStringLiteral("Breathing"),
        QStringLiteral("ColorCycle"),
    };
    for (int index = 0; index < knownEffects.size(); ++index) {
        if (effectName.compare(knownEffects.at(index), Qt::CaseInsensitive) == 0) {
            return index;
        }
    }
    return -1;
}

} // namespace

DeviceManager::DeviceManager(QObject* parent, QString profilesDirectory)
    : QObject(parent)
    , m_effectsEngine(std::make_unique<EffectsEngine>(this))
    , m_profileStore(std::move(profilesDirectory))
{
    connect(&m_activityLog, &ActivityLog::entryAdded, this, [this](const LogEntry& entry) {
        emit logMessage(entry.formatted());
    });
}

void DeviceManager::registerBackend(std::unique_ptr<RgbBackend> backend)
{
    m_backendRegistry.registerBackend(std::move(backend));
}

bool DeviceManager::activateBackend(const QString& id)
{
    return m_backendRegistry.activateBackend(id);
}

void DeviceManager::initializeBackends(const QString& backendId)
{
    m_effectsEngine->stopAll();
    m_devices.clear();
    m_confirmedWriteDeviceIds.clear();

    const QString requestedBackendId = backendId.trimmed();
    const QString activeBackendId = requestedBackendId.isEmpty() ? m_backendRegistry.activeBackendId() : requestedBackendId;

    if (!activeBackendId.isEmpty() && !m_backendRegistry.activateBackend(activeBackendId)) {
        m_activityLog.error(LogCategory::Backend, QStringLiteral("Could not activate backend '%1'.").arg(activeBackendId));
        emit devicesChanged();
        return;
    }

    if (m_backendRegistry.activeBackend() == nullptr) {
        const QStringList availableBackendIds = m_backendRegistry.availableBackendIds();
        if (!availableBackendIds.isEmpty()) {
            const bool activated = m_backendRegistry.activateBackend(availableBackendIds.constFirst());
            Q_UNUSED(activated)
        }
    }

    RgbBackend* backend = m_backendRegistry.activeBackend();
    if (backend == nullptr) {
        m_activityLog.error(LogCategory::Backend, QStringLiteral("No active backend available."));
        emit devicesChanged();
        return;
    }

    const BackendDescriptor backendDescriptor = backend->descriptor();
    m_activityLog.info(
        LogCategory::Backend,
        QStringLiteral("Registered backend '%1' (%2).").arg(backendDescriptor.displayName, backendDescriptor.id)
    );

    const PermissionResult probeResult = backend->probe();
    if (!probeResult.isGranted()) {
        m_activityLog.warning(LogCategory::Backend, probeResult.reason);
        emit devicesChanged();
        return;
    } else {
        m_activityLog.info(
            LogCategory::Backend,
            QStringLiteral("Backend '%1' probe OK.").arg(backendDescriptor.displayName)
        );
    }

    if (!backendDescriptor.capabilities.testFlag(BackendCapability::DiscoveryRead)) {
        m_activityLog.warning(
            LogCategory::Backend,
            QStringLiteral("Backend '%1' does not grant discovery access.").arg(backendDescriptor.displayName)
        );
        emit devicesChanged();
        return;
    }

    m_activityLog.info(
        LogCategory::Backend,
        QStringLiteral("Discovering devices via '%1'…").arg(backendDescriptor.displayName)
    );

    for (std::unique_ptr<RgbDevice>& device : backend->discoverDevices()) {
        if (device) {
            if (device->backendId().isEmpty()) {
                device->setBackendId(backendDescriptor.id);
            }
            registerDevice(std::move(device));
        }
    }

    emit devicesChanged();
    m_activityLog.info(LogCategory::Device, QStringLiteral("Loaded %1 device(s).").arg(deviceCount()));
}

void DeviceManager::initializeMockDevices()
{
    initializeBackends(QStringLiteral("mock"));
}

bool DeviceManager::dryRunEnabled() const
{
    return m_dryRunEnabled;
}

void DeviceManager::setDryRunEnabled(bool enabled)
{
    if (m_dryRunEnabled == enabled) {
        return;
    }

    m_dryRunEnabled = enabled;
    m_activityLog.info(
        LogCategory::Backend,
        enabled ? QStringLiteral("Dry-run mode enabled. Writes will be logged but not applied.")
                : QStringLiteral("Dry-run mode disabled. Writes will be applied.")
    );
    emit dryRunEnabledChanged();
}

BackendRegistry& DeviceManager::backendRegistry()
{
    return m_backendRegistry;
}

const BackendRegistry& DeviceManager::backendRegistry() const
{
    return m_backendRegistry;
}

int DeviceManager::deviceCount() const
{
    return static_cast<int>(m_devices.size());
}

RgbDevice* DeviceManager::deviceAt(int index)
{
    if (index < 0 || index >= deviceCount()) {
        return nullptr;
    }

    return m_devices.at(static_cast<std::size_t>(index)).get();
}

const RgbDevice* DeviceManager::deviceAt(int index) const
{
    if (index < 0 || index >= deviceCount()) {
        return nullptr;
    }

    return m_devices.at(static_cast<std::size_t>(index)).get();
}

const std::vector<std::unique_ptr<RgbDevice>>& DeviceManager::devices() const
{
    return m_devices;
}

bool DeviceManager::markDeviceRgbController(int deviceIndex, bool isRgbController)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    device->setRgbControllerOverride(isRgbController);
    saveRgbControllerOverride(*device);
    m_activityLog.info(
        LogCategory::Backend,
        isRgbController
            ? QStringLiteral("Marked '%1' as an RGB controller.").arg(device->name())
            : QStringLiteral("Removed '%1' from RGB controller filter.").arg(device->name())
    );
    emit devicesChanged();
    return true;
}

bool DeviceManager::clearDeviceRgbControllerOverride(int deviceIndex)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    device->clearRgbControllerOverride();
    removeRgbControllerOverride(*device);
    m_activityLog.info(LogCategory::Backend, QStringLiteral("Reset RGB controller filter override for '%1'.").arg(device->name()));
    emit devicesChanged();
    return true;
}

bool DeviceManager::confirmDeviceWrites(int deviceIndex)
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    const PermissionResult permission = PermissionGate::checkWrite(*device, BackendCapability::ZoneColorWrite);
    if (permission.status != PermissionStatus::RequiresConfirmation && !permission.isGranted()) {
        m_activityLog.warning(LogCategory::Permission, permission.reason);
        return false;
    }

    const bool alreadyConfirmed = m_confirmedWriteDeviceIds.contains(device->id());
    if (!alreadyConfirmed) {
        m_confirmedWriteDeviceIds.insert(device->id());
        m_activityLog.warning(
            LogCategory::Permission,
            QStringLiteral("Confirmed hardware writes for '%1' for this daemon session.").arg(device->name())
        );
        emit devicesChanged();
    }
    return true;
}

bool DeviceManager::revokeDeviceWrites(int deviceIndex)
{
    const RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    const bool removed = m_confirmedWriteDeviceIds.remove(device->id()) > 0;
    if (removed) {
        m_activityLog.info(
            LogCategory::Permission,
            QStringLiteral("Revoked hardware write confirmation for '%1'.").arg(device->name())
        );
        emit devicesChanged();
    }
    return removed;
}

bool DeviceManager::deviceWriteConfirmed(int deviceIndex) const
{
    const RgbDevice* device = deviceAt(deviceIndex);
    return device != nullptr && m_confirmedWriteDeviceIds.contains(device->id());
}

ActivityLog& DeviceManager::activityLog()
{
    return m_activityLog;
}

const ActivityLog& DeviceManager::activityLog() const
{
    return m_activityLog;
}

bool DeviceManager::updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount, QString* errorMessage)
{
    RgbDevice* device = deviceForZone(deviceIndex, zoneIndex);
    if (device == nullptr) {
        const QString message = QStringLiteral("Could not update selected zone.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_activityLog.error(LogCategory::Zone, message);
        return false;
    }

    const RgbZone& existingZone = device->zones().at(zoneIndex);
    const QString sanitizedName = name.trimmed();

    if (sanitizedName.isEmpty()) {
        const QString message = QStringLiteral("Zone name cannot be empty.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_activityLog.error(LogCategory::Zone, message);
        return false;
    }

    if (ledCount < 1) {
        const QString message = QStringLiteral("LED count must be at least 1.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_activityLog.error(LogCategory::Zone, message);
        return false;
    }

    const bool willChangeName = sanitizedName != existingZone.name();
    const bool willChangeLedCount = ledCount != existingZone.ledCount();

    const QString previousName = device->zones().at(zoneIndex).name();
    const int previousLedCount = device->zones().at(zoneIndex).ledCount();
    const bool metadataChanged = device->updateZoneMetadata(zoneIndex, sanitizedName, ledCount);
    const bool nameChanged = previousName != device->zones().at(zoneIndex).name();
    const bool ledCountChanged = previousLedCount != device->zones().at(zoneIndex).ledCount();

    if (!metadataChanged && (willChangeName || willChangeLedCount)) {
        const QString message = QStringLiteral("Could not update %1 / %2 metadata.").arg(device->name(), sanitizedName);
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_activityLog.error(LogCategory::Zone, message);
        return false;
    }

    if (!metadataChanged && !nameChanged && !ledCountChanged) {
        m_activityLog.info(
            LogCategory::Zone,
            QStringLiteral("%1 / %2 is already up to date.").arg(device->name(), sanitizedName)
        );
        return true;
    }

    m_activityLog.info(
        LogCategory::Zone,
        QStringLiteral("%1 / %2 updated (%3 LEDs).").arg(device->name(), sanitizedName).arg(ledCount)
    );
    if (nameChanged && previousName != sanitizedName) {
        m_activityLog.info(
            LogCategory::Zone,
            QStringLiteral("Renamed zone '%1' to '%2'.").arg(previousName, sanitizedName)
        );
    }
    if (ledCountChanged && previousLedCount != ledCount) {
        m_activityLog.info(
            LogCategory::Zone,
            QStringLiteral("Changed '%1' from %2 to %3 LED(s).")
                .arg(sanitizedName)
                .arg(previousLedCount)
                .arg(ledCount)
        );
    }

    return true;
}

bool DeviceManager::setZoneStaticColor(int deviceIndex, int zoneIndex, const RgbColor& color)
{
    return applyZoneEffect(deviceIndex, zoneIndex, RgbEffect(RgbEffectType::Static, color));
}

bool DeviceManager::applyZoneEffect(int deviceIndex, int zoneIndex, const RgbEffect& effect)
{
    RgbDevice* device = deviceForZone(deviceIndex, zoneIndex);
    if (device == nullptr) {
        m_activityLog.error(LogCategory::Effect, QStringLiteral("Could not apply effect: invalid device or zone."));
        return false;
    }

    const BackendCapability operation = effect.isAnimated() ? BackendCapability::ZoneEffectWrite : BackendCapability::ZoneColorWrite;
    const RgbZone& zone = device->zones().at(zoneIndex);
    QString dryRunSummary = effect.isAnimated()
        ? QStringLiteral("Would set %1 / %2 to %3 (speed %4x, brightness %5%).")
              .arg(device->name(), zone.name(), rgbEffectTypeToString(effect.type()))
              .arg(effect.speed())
              .arg(effect.brightness())
        : QStringLiteral("Would set %1 / %2 to %3 (brightness %4%).")
              .arg(device->name(), zone.name(), effect.color().toHexString())
              .arg(effect.brightness());
    const QString writePreview = device->previewZoneEffectWrite(zoneIndex, effect);
    if (!writePreview.isEmpty()) {
        dryRunSummary.append(QStringLiteral(" %1").arg(writePreview));
    }

    const WriteGate::Outcome outcome = WriteGate::evaluate(
        m_dryRunEnabled,
        *device,
        operation,
        m_activityLog,
        dryRunSummary,
        deviceWriteConfirmed(deviceIndex)
    );
    if (!outcome.reportSuccess) {
        return false;
    }
    if (!outcome.applyWrite) {
        return true;
    }

    if (!device->applyZoneEffect(zoneIndex, effect)) {
        const QString hardwareStatus = device->lastHardwareWriteStatus();
        m_activityLog.error(
            LogCategory::Effect,
            hardwareStatus.isEmpty()
                ? QStringLiteral("Could not apply effect to selected zone.")
                : QStringLiteral("Could not apply effect to selected zone. %1").arg(hardwareStatus)
        );
        return false;
    }
    const QString hardwareStatus = device->lastHardwareWriteStatus();
    if (!hardwareStatus.isEmpty()) {
        m_activityLog.info(LogCategory::Backend, hardwareStatus);
    }

    if (effect.isAnimated()) {
        if (device->usesLocalFrameRendering()) {
            m_effectsEngine->startZone(deviceIndex, zoneIndex);
        } else {
            m_effectsEngine->stopZone(deviceIndex, zoneIndex);
        }
        m_activityLog.info(
            LogCategory::Effect,
            QStringLiteral("%1 / %2 set to %3 (speed %4x, brightness %5%).")
                .arg(device->name(), zone.name(), rgbEffectTypeToString(effect.type()))
                .arg(effect.speed())
                .arg(effect.brightness())
        );
    } else {
        m_effectsEngine->stopZone(deviceIndex, zoneIndex);
        if (device->usesLocalFrameRendering()) {
            const QVector<RgbColor> frame = EffectsEngine::computeFrame(effect, zone.ledCount(), 0.0);
            if (!frame.isEmpty()) {
                const bool frameApplied = device->applyZoneFrame(zoneIndex, frame);
                Q_UNUSED(frameApplied)
            }
        }
        m_activityLog.info(
            LogCategory::Effect,
            QStringLiteral("%1 / %2 set to %3 (brightness %4%).")
                .arg(device->name(), zone.name(), effect.color().toHexString())
                .arg(effect.brightness())
        );
    }

    emit zoneChanged(deviceIndex, zoneIndex);
    emit zoneColorChanged(deviceIndex, zoneIndex);
    return true;
}

bool DeviceManager::applyAllOff(int deviceIndex, QString* errorMessage)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        const QString message = QStringLiteral("Could not turn off selected device.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_activityLog.error(LogCategory::Effect, message);
        return false;
    }

    const QString dryRunSummary = QStringLiteral("Would turn off all zones on %1.").arg(device->name());
    const WriteGate::Outcome outcome = WriteGate::evaluate(
        m_dryRunEnabled,
        *device,
        BackendCapability::ZoneColorWrite,
        m_activityLog,
        dryRunSummary,
        deviceWriteConfirmed(deviceIndex)
    );
    if (!outcome.reportSuccess) {
        return false;
    }
    if (!outcome.applyWrite) {
        return true;
    }

    if (!device->applyAllOff()) {
        const QString message = QStringLiteral("Could not turn off selected device.");
        if (errorMessage != nullptr) {
            const QString hardwareStatus = device->lastHardwareWriteStatus();
            *errorMessage = hardwareStatus.isEmpty() ? message : QStringLiteral("%1 %2").arg(message, hardwareStatus);
        }
        const QString hardwareStatus = device->lastHardwareWriteStatus();
        m_activityLog.error(
            LogCategory::Effect,
            hardwareStatus.isEmpty() ? message : QStringLiteral("%1 %2").arg(message, hardwareStatus)
        );
        return false;
    }
    const QString hardwareStatus = device->lastHardwareWriteStatus();
    if (!hardwareStatus.isEmpty()) {
        m_activityLog.info(LogCategory::Backend, hardwareStatus);
    }

    m_effectsEngine->stopAll();
    for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
        emit zoneChanged(deviceIndex, zoneIndex);
        emit zoneColorChanged(deviceIndex, zoneIndex);
    }
    m_activityLog.warning(LogCategory::Effect, QStringLiteral("%1 all zones set to off.").arg(device->name()));
    return true;
}

void DeviceManager::paintZoneFrame(int deviceIndex, int zoneIndex, const QVector<RgbColor>& colors)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return;
    }

    if (m_dryRunEnabled) {
        return;
    }

    const PermissionResult permission = PermissionGate::checkWrite(*device, BackendCapability::ZoneEffectWrite);
    if (!permission.isGranted()) {
        m_effectsEngine->stopZone(deviceIndex, zoneIndex);
        m_activityLog.warning(LogCategory::Permission, permission.reason);
        return;
    }

    if (device->applyZoneFrame(zoneIndex, colors)) {
        emit zoneFrameUpdated(deviceIndex, zoneIndex);
    }
}

bool DeviceManager::saveProfile(const QString& profileName, QString* errorMessage)
{
    const QString normalizedName = normalizeProfileName(profileName);
    QJsonArray devicesJson;
    for (const std::unique_ptr<RgbDevice>& device : m_devices) {
        QJsonArray zonesJson;
        int zoneIndex = 0;
        for (const RgbZone& zone : device->zones()) {
            zonesJson.append(QJsonObject {
                {QStringLiteral("index"), zoneIndex},
                {QStringLiteral("name"), zone.name()},
                {QStringLiteral("type"), zone.typeName()},
                {QStringLiteral("ledCount"), zone.ledCount()},
                {QStringLiteral("color"), zone.effect().color().toHexString()},
                {QStringLiteral("rgb"), zone.effect().color().toJson()},
                {QStringLiteral("effect"), zone.effect().toJson()},
            });
            ++zoneIndex;
        }

        devicesJson.append(QJsonObject {
            {QStringLiteral("id"), device->id()},
            {QStringLiteral("name"), device->name()},
            {QStringLiteral("vendor"), device->vendor()},
            {QStringLiteral("type"), device->typeName()},
            {QStringLiteral("zones"), zonesJson},
        });
    }

    const QJsonObject root {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("application"), QStringLiteral("LumaCore")},
        {QStringLiteral("profileName"), normalizedName},
        {QStringLiteral("devices"), devicesJson},
    };

    QString storeError;
    if (!m_profileStore.save(normalizedName, root, &storeError)) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        m_activityLog.error(LogCategory::Profile, storeError);
        return false;
    }

    m_activityLog.info(LogCategory::Profile, QStringLiteral("Saved profile '%1'.").arg(normalizedName));
    return true;
}

bool DeviceManager::loadProfile(const QString& profileName, QString* errorMessage)
{
    const QVariantMap report = applyProfileWithReport(profileName);
    const bool success = report.value(QStringLiteral("success")).toBool();
    if (!success && errorMessage != nullptr) {
        *errorMessage = report.value(QStringLiteral("error")).toString();
    }
    return success;
}

QVariantMap DeviceManager::applyProfileWithReport(const QString& profileName)
{
    const QString normalizedName = normalizeProfileName(profileName);
    QJsonObject profile;
    QString storeError;
    if (!m_profileStore.load(normalizedName, &profile, &storeError)) {
        m_activityLog.error(LogCategory::Profile, storeError);
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("partial"), false},
            {QStringLiteral("profileName"), normalizedName},
            {QStringLiteral("error"), storeError},
        };
    }

    int totalZones = 0;
    int appliedZones = 0;
    int skippedMissingDeviceZones = 0;
    int skippedMissingZones = 0;
    int skippedInvalidZones = 0;
    int skippedUnsupportedZones = 0;
    int failedZones = 0;
    QStringList details;

    const QJsonArray devicesJson = profile.value(QStringLiteral("devices")).toArray();
    for (const QJsonValue& deviceValue : devicesJson) {
        const QJsonObject deviceObject = deviceValue.toObject();
        const QString deviceId = deviceObject.value(QStringLiteral("id")).toString();
        const QString storedDeviceName = deviceObject.value(QStringLiteral("name")).toString(deviceId);
        const QJsonArray zonesJson = deviceObject.value(QStringLiteral("zones")).toArray();
        totalZones += zonesJson.size();

        int deviceIndex = -1;
        for (int candidateIndex = 0; candidateIndex < deviceCount(); ++candidateIndex) {
            RgbDevice* device = deviceAt(candidateIndex);
            if (device != nullptr && device->id() == deviceId) {
                deviceIndex = candidateIndex;
                break;
            }
        }

        if (deviceIndex < 0) {
            skippedMissingDeviceZones += zonesJson.size();
            details.append(QStringLiteral("Skipped missing device: %1 (%2 zone(s))").arg(storedDeviceName).arg(zonesJson.size()));
            continue;
        }

        RgbDevice* device = deviceAt(deviceIndex);
        for (const QJsonValue& zoneValue : zonesJson) {
            const QJsonObject zoneObject = zoneValue.toObject();
            const QString zoneName = zoneObject.value(QStringLiteral("name")).toString();
            const bool hasEffect = zoneObject.contains(QStringLiteral("effect"));
            const int effectType = storedEffectType(zoneObject);
            bool colorOk = false;
            const RgbColor color = RgbColor::fromHexString(zoneObject.value(QStringLiteral("color")).toString(), &colorOk);
            if ((!hasEffect && !colorOk) || effectType < 0) {
                ++skippedInvalidZones;
                details.append(QStringLiteral("Skipped invalid zone: %1 / %2").arg(storedDeviceName, zoneName));
                continue;
            }

            int matchedZoneIndex = -1;
            for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
                if (device->zones().at(zoneIndex).name() == zoneName) {
                    matchedZoneIndex = zoneIndex;
                    break;
                }
            }
            if (matchedZoneIndex < 0) {
                const int storedZoneIndex = zoneObject.value(QStringLiteral("index")).toInt(-1);
                if (storedZoneIndex >= 0 && storedZoneIndex < device->zones().size()) {
                    matchedZoneIndex = storedZoneIndex;
                }
            }
            if (matchedZoneIndex < 0) {
                ++skippedMissingZones;
                details.append(QStringLiteral("Skipped missing zone: %1 / %2").arg(storedDeviceName, zoneName));
                continue;
            }
            if (!device->supportsEffect(effectType)) {
                ++skippedUnsupportedZones;
                details.append(QStringLiteral("Skipped unsupported effect: %1 / %2").arg(storedDeviceName, zoneName));
                continue;
            }

            const RgbEffect effect = hasEffect
                ? RgbEffect::fromJson(zoneObject.value(QStringLiteral("effect")).toObject())
                : RgbEffect(RgbEffectType::Static, color);
            const int ledCount =
                zoneObject.value(QStringLiteral("ledCount")).toInt(device->zones().at(matchedZoneIndex).ledCount());
            if (!updateZone(deviceIndex, matchedZoneIndex, zoneName, qMax(1, ledCount))
                || !applyZoneEffect(deviceIndex, matchedZoneIndex, effect)) {
                ++failedZones;
                details.append(QStringLiteral("Failed to apply zone: %1 / %2").arg(storedDeviceName, zoneName));
                continue;
            }
            ++appliedZones;
        }
    }

    const int skippedZones =
        skippedMissingDeviceZones + skippedMissingZones + skippedInvalidZones + skippedUnsupportedZones + failedZones;
    const bool success = appliedZones > 0;
    const bool partial = success && skippedZones > 0;
    const QString summary = success
        ? QStringLiteral("Applied %1 of %2 zone(s); skipped or failed %3.")
              .arg(appliedZones)
              .arg(totalZones)
              .arg(skippedZones)
        : QStringLiteral("Applied 0 of %1 zone(s).").arg(totalZones);
    const QString failureError = QStringLiteral("Profile did not match any available mock zones.");

    if (appliedZones == 0) {
        m_activityLog.error(LogCategory::Profile, summary);
    } else {
        m_activityLog.info(
            LogCategory::Profile,
            QStringLiteral("Loaded profile '%1'. %2").arg(normalizedName, summary)
        );
    }

    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("partial"), partial},
        {QStringLiteral("profileName"), normalizedName},
        {QStringLiteral("totalZones"), totalZones},
        {QStringLiteral("appliedZones"), appliedZones},
        {QStringLiteral("skippedZones"), skippedZones},
        {QStringLiteral("missingDeviceZones"), skippedMissingDeviceZones},
        {QStringLiteral("missingZones"), skippedMissingZones},
        {QStringLiteral("invalidZones"), skippedInvalidZones},
        {QStringLiteral("unsupportedZones"), skippedUnsupportedZones},
        {QStringLiteral("failedZones"), failedZones},
        {QStringLiteral("summary"), summary},
        {QStringLiteral("error"), success ? QString() : failureError},
        {QStringLiteral("details"), details},
    };
}

bool DeviceManager::deleteProfile(const QString& profileName, QString* errorMessage)
{
    const QString normalizedName = normalizeProfileName(profileName);
    QString storeError;
    if (!m_profileStore.remove(normalizedName, &storeError)) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        m_activityLog.error(LogCategory::Profile, storeError);
        return false;
    }

    m_activityLog.info(LogCategory::Profile, QStringLiteral("Deleted profile '%1'.").arg(normalizedName));
    return true;
}

bool DeviceManager::renameProfile(const QString& oldProfileName, const QString& newProfileName, QString* errorMessage)
{
    const QString normalizedOldName = normalizeProfileName(oldProfileName);
    const QString normalizedNewName = normalizeProfileName(newProfileName);

    QString storeError;
    if (!m_profileStore.rename(normalizedOldName, normalizedNewName, &storeError)) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        if (normalizedOldName == normalizedNewName) {
            m_activityLog.warning(LogCategory::Profile, storeError);
        } else {
            m_activityLog.error(LogCategory::Profile, storeError);
        }
        return false;
    }

    m_activityLog.info(
        LogCategory::Profile,
        QStringLiteral("Renamed profile '%1' to '%2'.").arg(normalizedOldName, normalizedNewName)
    );
    return true;
}

bool DeviceManager::importProfile(const QString& sourcePath, QString* importedProfileName, QString* errorMessage)
{
    QString storeError;
    QString importedName;
    if (!m_profileStore.importFile(sourcePath, &importedName, &storeError)) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        m_activityLog.error(LogCategory::Profile, storeError);
        return false;
    }

    if (importedProfileName != nullptr) {
        *importedProfileName = importedName;
    }
    m_activityLog.info(LogCategory::Profile, QStringLiteral("Imported profile '%1'.").arg(importedName));
    return true;
}

bool DeviceManager::exportProfile(const QString& profileName, const QString& destinationPath, QString* errorMessage)
{
    const QString normalizedName = normalizeProfileName(profileName);
    QString storeError;
    if (!m_profileStore.exportFile(normalizedName, destinationPath, &storeError)) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        m_activityLog.error(LogCategory::Profile, storeError);
        return false;
    }

    m_activityLog.info(LogCategory::Profile, QStringLiteral("Exported profile '%1'.").arg(normalizedName));
    return true;
}

QVariantMap DeviceManager::profileCompatibility(const QString& profileName) const
{
    QJsonObject profile;
    QString errorMessage;
    if (!m_profileStore.load(normalizeProfileName(profileName), &profile, &errorMessage)) {
        return {
            {QStringLiteral("valid"), false},
            {QStringLiteral("canApply"), false},
            {QStringLiteral("error"), errorMessage},
        };
    }

    int storedDevices = 0;
    int matchedDevices = 0;
    int missingDevices = 0;
    int totalZones = 0;
    int matchedZones = 0;
    int applicableZones = 0;
    int missingZones = 0;
    int unsupportedEffects = 0;
    QStringList details;

    const QJsonArray devicesJson = profile.value(QStringLiteral("devices")).toArray();
    storedDevices = devicesJson.size();
    for (const QJsonValue& deviceValue : devicesJson) {
        const QJsonObject deviceObject = deviceValue.toObject();
        const QString deviceId = deviceObject.value(QStringLiteral("id")).toString();
        const QString storedDeviceName = deviceObject.value(QStringLiteral("name")).toString(deviceId);
        const RgbDevice* matchedDevice = nullptr;
        for (const std::unique_ptr<RgbDevice>& device : m_devices) {
            if (device != nullptr && device->id() == deviceId) {
                matchedDevice = device.get();
                break;
            }
        }

        const QJsonArray zonesJson = deviceObject.value(QStringLiteral("zones")).toArray();
        totalZones += zonesJson.size();
        if (matchedDevice == nullptr) {
            ++missingDevices;
            missingZones += zonesJson.size();
            details.append(QStringLiteral("Missing device: %1").arg(storedDeviceName));
            continue;
        }
        ++matchedDevices;

        for (const QJsonValue& zoneValue : zonesJson) {
            const QJsonObject zoneObject = zoneValue.toObject();
            const QString zoneName = zoneObject.value(QStringLiteral("name")).toString();
            int matchedZoneIndex = -1;
            for (int zoneIndex = 0; zoneIndex < matchedDevice->zones().size(); ++zoneIndex) {
                if (matchedDevice->zones().at(zoneIndex).name() == zoneName) {
                    matchedZoneIndex = zoneIndex;
                    break;
                }
            }
            if (matchedZoneIndex < 0) {
                const int storedZoneIndex = zoneObject.value(QStringLiteral("index")).toInt(-1);
                if (storedZoneIndex >= 0 && storedZoneIndex < matchedDevice->zones().size()) {
                    matchedZoneIndex = storedZoneIndex;
                }
            }

            if (matchedZoneIndex < 0) {
                ++missingZones;
                details.append(QStringLiteral("Missing zone: %1 / %2").arg(storedDeviceName, zoneName));
                continue;
            }
            ++matchedZones;

            const QJsonObject effectObject = zoneObject.value(QStringLiteral("effect")).toObject();
            const QString effectName = effectObject.isEmpty()
                ? QStringLiteral("Static")
                : effectObject.value(QStringLiteral("type")).toString(QStringLiteral("Static"));
            const int effectType = storedEffectType(zoneObject);

            if (effectType < 0 || !matchedDevice->supportsEffect(effectType)) {
                ++unsupportedEffects;
                details.append(
                    QStringLiteral("Unsupported effect: %1 / %2 (%3)")
                        .arg(storedDeviceName, zoneName, effectName)
                );
                continue;
            }
            ++applicableZones;
        }
    }

    const QString summary =
        QStringLiteral("%1 of %2 zone(s) can be applied. %3 missing device(s), %4 missing zone(s), %5 unsupported effect(s).")
            .arg(applicableZones)
            .arg(totalZones)
            .arg(missingDevices)
            .arg(missingZones)
            .arg(unsupportedEffects);

    return {
        {QStringLiteral("valid"), true},
        {QStringLiteral("canApply"), applicableZones > 0},
        {QStringLiteral("profileName"), normalizeProfileName(profileName)},
        {QStringLiteral("storedDevices"), storedDevices},
        {QStringLiteral("matchedDevices"), matchedDevices},
        {QStringLiteral("missingDevices"), missingDevices},
        {QStringLiteral("totalZones"), totalZones},
        {QStringLiteral("matchedZones"), matchedZones},
        {QStringLiteral("applicableZones"), applicableZones},
        {QStringLiteral("missingZones"), missingZones},
        {QStringLiteral("unsupportedEffects"), unsupportedEffects},
        {QStringLiteral("summary"), summary},
        {QStringLiteral("details"), details},
    };
}

QStringList DeviceManager::profileNames() const
{
    return m_profileStore.names();
}

QString DeviceManager::profilesDirectoryPath() const
{
    return m_profileStore.directoryPath();
}

RgbDevice* DeviceManager::deviceForZone(int deviceIndex, int zoneIndex)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return nullptr;
    }
    return device;
}

void DeviceManager::registerDevice(std::unique_ptr<RgbDevice> device)
{
    if (!device) {
        return;
    }

    applySavedRgbControllerOverride(*device);
    const int deviceIndex = deviceCount();
    connect(device.get(), &RgbDevice::zoneChanged, this, [this, deviceIndex](int zoneIndex) {
        emit zoneChanged(deviceIndex, zoneIndex);
        emit zoneColorChanged(deviceIndex, zoneIndex);
    });

    m_devices.push_back(std::move(device));
}

void DeviceManager::applySavedRgbControllerOverride(RgbDevice& device)
{
    QSettings settings;
    const QString key = rgbControllerOverrideKey(device.id());
    if (settings.contains(key)) {
        device.setRgbControllerOverride(settings.value(key).toBool());
    }
}

void DeviceManager::saveRgbControllerOverride(const RgbDevice& device)
{
    QSettings settings;
    settings.setValue(rgbControllerOverrideKey(device.id()), device.rgbControllerOverride());
}

void DeviceManager::removeRgbControllerOverride(const RgbDevice& device)
{
    QSettings settings;
    settings.remove(rgbControllerOverrideKey(device.id()));
}

QString DeviceManager::rgbControllerOverrideKey(const QString& deviceId)
{
    QString normalizedId = deviceId;
    normalizedId.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    return QStringLiteral("discovery/rgbControllerOverrides/%1").arg(normalizedId);
}

QString DeviceManager::normalizeProfileName(const QString& profileName)
{
    return ProfileStore::normalizeName(profileName);
}

} // namespace lumacore
