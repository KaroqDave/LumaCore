// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/daemon/DaemonRgbDevice.h"

#include "core/PermissionGate.h"
#include "ipc/DaemonProtocol.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>

#include <utility>

namespace lumacore {

namespace {

QString deviceString(const QJsonObject& snapshot, const QString& key, const QString& fallback = {})
{
    const QString value = snapshot.value(key).toString();
    return value.isEmpty() ? fallback : value;
}

QHash<int, DaemonRgbDevice::EffectSupport> effectSupportFromJson(const QJsonArray& effectSupport)
{
    QHash<int, DaemonRgbDevice::EffectSupport> supportByType;
    for (const QJsonValue& supportValue : effectSupport) {
        const QJsonObject supportObject = supportValue.toObject();
        supportByType.insert(
            supportObject.value(QStringLiteral("effectType")).toInt(-1),
            {
                supportObject.value(QStringLiteral("supported")).toBool(false),
                supportObject.value(QStringLiteral("speed")).toBool(false),
                supportObject.value(QStringLiteral("brightness")).toBool(false),
            }
        );
    }
    return supportByType;
}

bool daemonCallSucceeded(const DaemonCallResult& result)
{
    return result.ok && result.result.value(QStringLiteral("success")).toBool(false);
}

QString daemonCallError(const DaemonCallResult& result)
{
    return result.ok
        ? result.result.value(QStringLiteral("error")).toString()
        : result.error;
}

QString daemonHardwareStatus(const DaemonCallResult& result)
{
    return result.result.value(QStringLiteral("hardwareStatus")).toString(daemonCallError(result));
}

} // namespace

DaemonRgbDevice::DaemonRgbDevice(QJsonObject snapshot, std::shared_ptr<DaemonClient> client, QObject* parent)
    : RgbDevice(
          deviceString(snapshot, QStringLiteral("id"), QStringLiteral("daemon-device")),
          deviceString(snapshot, QStringLiteral("name"), QStringLiteral("Daemon RGB Device")),
          deviceString(snapshot, QStringLiteral("vendor"), QStringLiteral("Unknown")),
          rgbDeviceTypeFromString(snapshot.value(QStringLiteral("type")).toString()),
          parent
      )
    , m_daemonDeviceIndex(snapshot.value(QStringLiteral("index")).toInt(-1))
    , m_discoveryIdentity(snapshot.value(QStringLiteral("discoveryIdentity")).toString())
    , m_discoverySupportStage(snapshot.value(QStringLiteral("discoverySupportStage")).toString())
    , m_discoverySupportStatus(snapshot.value(QStringLiteral("discoverySupportStatus")).toString())
    , m_discoverySupportFamily(snapshot.value(QStringLiteral("discoverySupportFamily")).toString())
    , m_discoverySupportNotes(snapshot.value(QStringLiteral("discoverySupportNotes")).toString())
    , m_discoveryCataloged(snapshot.value(QStringLiteral("discoveryCataloged")).toBool(false))
    , m_discoveryWriteCapableBackend(snapshot.value(QStringLiteral("discoveryWriteCapableBackend")).toBool(false))
    , m_capabilities(capabilitiesFromJson(snapshot.value(QStringLiteral("capabilities")).toArray()))
    , m_permission(permissionResultFromJson(snapshot.value(QStringLiteral("permission")).toObject()))
    , m_writeConfirmed(snapshot.value(QStringLiteral("writeConfirmed")).toBool(false))
    , m_lastHardwareWriteStatus(snapshot.value(QStringLiteral("lastHardwareWriteStatus")).toString())
    , m_client(std::move(client))
{
    setBackendId(deviceString(snapshot, QStringLiteral("backendId"), QStringLiteral("daemon")));
    setLikelyRgbController(snapshot.value(QStringLiteral("likelyRgbController")).toBool(false));
    if (snapshot.value(QStringLiteral("hasRgbControllerOverride")).toBool(false)) {
        setRgbControllerOverride(snapshot.value(QStringLiteral("rgbControllerOverride")).toBool(false));
    }

    const QJsonObject permissions = snapshot.value(QStringLiteral("permissions")).toObject();
    for (auto it = permissions.constBegin(); it != permissions.constEnd(); ++it) {
        m_permissions.insert(it.key(), permissionResultFromJson(it.value().toObject()));
    }
    if (m_permissions.isEmpty()) {
        m_permissions.insert(backendCapabilityToString(BackendCapability::ZoneColorWrite), m_permission);
        m_permissions.insert(backendCapabilityToString(BackendCapability::ZoneEffectWrite), m_permission);
    }

    m_effectSupport = effectSupportFromJson(snapshot.value(QStringLiteral("effectSupport")).toArray());

    const QJsonArray zones = snapshot.value(QStringLiteral("zones")).toArray();
    m_zoneEffectSupport.reserve(zones.size());
    for (const QJsonValue& zoneValue : zones) {
        const QJsonObject zone = zoneValue.toObject();
        bool colorOk = false;
        const RgbColor color = RgbColor::fromHexString(zone.value(QStringLiteral("color")).toString(), &colorOk);
        RgbZone rgbZone(
            zone.value(QStringLiteral("name")).toString(QStringLiteral("Zone")),
            rgbZoneTypeFromString(zone.value(QStringLiteral("type")).toString()),
            zone.value(QStringLiteral("ledCount")).toInt(1),
            colorOk ? color : RgbColor()
        );
        rgbZone.setEffect(effectFromJson(zone.value(QStringLiteral("effect")).toObject()));
        mutableZones().append(rgbZone);
        m_zoneEffectSupport.append(effectSupportFromJson(zone.value(QStringLiteral("effectSupport")).toArray()));
    }
}

QString DaemonRgbDevice::discoveryIdentity() const
{
    return m_discoveryIdentity;
}

QString DaemonRgbDevice::discoverySupportStage() const
{
    return m_discoverySupportStage;
}

QString DaemonRgbDevice::discoverySupportStatus() const
{
    return m_discoverySupportStatus;
}

QString DaemonRgbDevice::discoverySupportFamily() const
{
    return m_discoverySupportFamily;
}

QString DaemonRgbDevice::discoverySupportNotes() const
{
    return m_discoverySupportNotes;
}

bool DaemonRgbDevice::discoveryCataloged() const
{
    return m_discoveryCataloged;
}

bool DaemonRgbDevice::discoveryWriteCapableBackend() const
{
    return m_discoveryWriteCapableBackend;
}

bool DaemonRgbDevice::setZoneStaticColor(int zoneIndex, const RgbColor& color)
{
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }

    mutableZones()[zoneIndex].setColor(color);
    emit zoneChanged(zoneIndex);
    return true;
}

bool DaemonRgbDevice::applyZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    m_lastHardwareWriteStatus.clear();
    if (m_client == nullptr || zoneIndex < 0 || zoneIndex >= zones().size()) {
        m_lastHardwareWriteStatus = QStringLiteral("Daemon write skipped: invalid proxy device or zone.");
        return false;
    }

    const DaemonCallResult result = m_client->call(daemonMethodName(DaemonMethod::ApplyEffect), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
        {QStringLiteral("zoneIndex"), zoneIndex},
        {QStringLiteral("effect"), effect.toJson()},
        {QStringLiteral("dryRunEnabled"), false},
    });
    m_lastHardwareWriteStatus = daemonHardwareStatus(result);
    if (!daemonCallSucceeded(result)) {
        return false;
    }

    applyLocalZoneEffect(zoneIndex, effect);
    return true;
}

quint64 DaemonRgbDevice::applyZoneEffectAsync(
    int zoneIndex,
    const RgbEffect& effect,
    bool updateLocalState,
    OperationHandler handler
)
{
    m_lastHardwareWriteStatus.clear();
    if (m_client == nullptr || zoneIndex < 0 || zoneIndex >= zones().size()) {
        m_lastHardwareWriteStatus = QStringLiteral("Daemon write skipped: invalid proxy device or zone.");
        if (handler) {
            handler(false, m_lastHardwareWriteStatus);
        }
        return 0;
    }

    const QPointer<DaemonRgbDevice> self(this);
    return m_client->callAsync(
        daemonMethodName(DaemonMethod::ApplyEffect),
        {
            {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
            {QStringLiteral("zoneIndex"), zoneIndex},
            {QStringLiteral("effect"), effect.toJson()},
            {QStringLiteral("dryRunEnabled"), !updateLocalState},
        },
        [self, zoneIndex, effect, updateLocalState, handler = std::move(handler)](DaemonCallResult result) mutable {
            const bool success = daemonCallSucceeded(result);
            QString error = daemonCallError(result);
            if (self != nullptr) {
                self->m_lastHardwareWriteStatus = daemonHardwareStatus(result);
                if (success && updateLocalState) {
                    self->applyLocalZoneEffect(zoneIndex, effect);
                } else if (error.isEmpty()) {
                    error = self->m_lastHardwareWriteStatus;
                }
            }
            if (handler) {
                handler(success, error);
            }
        }
    );
}

bool DaemonRgbDevice::applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(colors)
    return false;
}

bool DaemonRgbDevice::applyAllOff()
{
    m_lastHardwareWriteStatus.clear();
    if (m_client == nullptr || m_daemonDeviceIndex < 0) {
        m_lastHardwareWriteStatus = QStringLiteral("Daemon all-off skipped: invalid proxy device.");
        return false;
    }

    const DaemonCallResult result = m_client->call(daemonMethodName(DaemonMethod::AllOff), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
        {QStringLiteral("dryRunEnabled"), false},
    });
    m_lastHardwareWriteStatus = daemonHardwareStatus(result);
    if (!daemonCallSucceeded(result)) {
        return false;
    }

    applyLocalAllOff();
    return true;
}

quint64 DaemonRgbDevice::applyAllOffAsync(bool updateLocalState, OperationHandler handler)
{
    m_lastHardwareWriteStatus.clear();
    if (m_client == nullptr || m_daemonDeviceIndex < 0) {
        m_lastHardwareWriteStatus = QStringLiteral("Daemon all-off skipped: invalid proxy device.");
        if (handler) {
            handler(false, m_lastHardwareWriteStatus);
        }
        return 0;
    }

    const QPointer<DaemonRgbDevice> self(this);
    return m_client->callAsync(
        daemonMethodName(DaemonMethod::AllOff),
        {
            {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
            {QStringLiteral("dryRunEnabled"), !updateLocalState},
        },
        [self, updateLocalState, handler = std::move(handler)](DaemonCallResult result) mutable {
            const bool success = daemonCallSucceeded(result);
            QString error = daemonCallError(result);
            if (self != nullptr) {
                self->m_lastHardwareWriteStatus = daemonHardwareStatus(result);
                if (success && updateLocalState) {
                    self->applyLocalAllOff();
                } else if (error.isEmpty()) {
                    error = self->m_lastHardwareWriteStatus;
                }
            }
            if (handler) {
                handler(success, error);
            }
        }
    );
}

bool DaemonRgbDevice::updateZoneMetadata(int zoneIndex, const QString& name, int ledCount)
{
    if (m_client == nullptr || zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }

    const DaemonCallResult result = m_client->call(daemonMethodName(DaemonMethod::UpdateZone), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
        {QStringLiteral("zoneIndex"), zoneIndex},
        {QStringLiteral("name"), name.trimmed()},
        {QStringLiteral("ledCount"), ledCount},
    });
    if (!daemonCallSucceeded(result)) {
        return false;
    }

    const bool changedName = setZoneName(zoneIndex, name);
    const bool changedLedCount = setZoneLedCount(zoneIndex, ledCount);
    return changedName || changedLedCount;
}

quint64 DaemonRgbDevice::updateZoneMetadataAsync(
    int zoneIndex,
    const QString& name,
    int ledCount,
    OperationHandler handler
)
{
    if (m_client == nullptr || zoneIndex < 0 || zoneIndex >= zones().size()) {
        if (handler) {
            handler(false, QStringLiteral("Could not update selected zone."));
        }
        return 0;
    }

    const QString sanitizedName = name.trimmed();
    const QPointer<DaemonRgbDevice> self(this);
    return m_client->callAsync(
        daemonMethodName(DaemonMethod::UpdateZone),
        {
            {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
            {QStringLiteral("zoneIndex"), zoneIndex},
            {QStringLiteral("name"), sanitizedName},
            {QStringLiteral("ledCount"), ledCount},
        },
        [self, zoneIndex, sanitizedName, ledCount, handler = std::move(handler)](DaemonCallResult result) mutable {
            const bool success = daemonCallSucceeded(result);
            QString error = daemonCallError(result);
            if (success && self != nullptr) {
                const bool changedName = self->setZoneName(zoneIndex, sanitizedName);
                const bool changedLedCount = self->setZoneLedCount(zoneIndex, ledCount);
                if (!changedName && !changedLedCount) {
                    emit self->zoneChanged(zoneIndex);
                }
            }
            if (handler) {
                handler(success, error);
            }
        }
    );
}

bool DaemonRgbDevice::usesLocalFrameRendering() const
{
    return false;
}

QString DaemonRgbDevice::previewZoneEffectWrite(int zoneIndex, const RgbEffect& effect) const
{
    if (m_client == nullptr || zoneIndex < 0 || zoneIndex >= zones().size()) {
        return {};
    }

    const DaemonCallResult result = m_client->call(daemonMethodName(DaemonMethod::PreviewEffect), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
        {QStringLiteral("zoneIndex"), zoneIndex},
        {QStringLiteral("effect"), effect.toJson()},
    });
    if (!daemonCallSucceeded(result)) {
        return {};
    }

    return result.result.value(QStringLiteral("preview")).toString();
}

QString DaemonRgbDevice::lastHardwareWriteStatus() const
{
    return m_lastHardwareWriteStatus;
}

bool DaemonRgbDevice::writeConfirmed() const
{
    return m_writeConfirmed;
}

bool DaemonRgbDevice::writeRequiresConfirmation() const
{
    return PermissionGate::writeRequiresConfirmation(*this);
}

bool DaemonRgbDevice::supportsEffect(int effectType) const
{
    return m_effectSupport.value(effectType).supported;
}

bool DaemonRgbDevice::supportsEffectSpeed(int effectType) const
{
    return m_effectSupport.value(effectType).speed;
}

bool DaemonRgbDevice::supportsEffectBrightness(int effectType) const
{
    return m_effectSupport.value(effectType).brightness;
}

bool DaemonRgbDevice::supportsZoneEffect(int zoneIndex, int effectType) const
{
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }
    if (zoneIndex < m_zoneEffectSupport.size() && !m_zoneEffectSupport.at(zoneIndex).isEmpty()) {
        return m_zoneEffectSupport.at(zoneIndex).value(effectType).supported;
    }
    return supportsEffect(effectType);
}

bool DaemonRgbDevice::supportsZoneEffectSpeed(int zoneIndex, int effectType) const
{
    if (!supportsZoneEffect(zoneIndex, effectType)) {
        return false;
    }
    if (zoneIndex < m_zoneEffectSupport.size() && !m_zoneEffectSupport.at(zoneIndex).isEmpty()) {
        return m_zoneEffectSupport.at(zoneIndex).value(effectType).speed;
    }
    return supportsEffectSpeed(effectType);
}

bool DaemonRgbDevice::supportsZoneEffectBrightness(int zoneIndex, int effectType) const
{
    if (!supportsZoneEffect(zoneIndex, effectType)) {
        return false;
    }
    if (zoneIndex < m_zoneEffectSupport.size() && !m_zoneEffectSupport.at(zoneIndex).isEmpty()) {
        return m_zoneEffectSupport.at(zoneIndex).value(effectType).brightness;
    }
    return supportsEffectBrightness(effectType);
}

void DaemonRgbDevice::setWriteConfirmed(bool confirmed)
{
    m_writeConfirmed = confirmed;
}

void DaemonRgbDevice::applyLocalZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    setZoneEffect(zoneIndex, effect);
    if (!effect.isAnimated()) {
        mutableZones()[zoneIndex].setColor(effect.color());
    }
    emit zoneChanged(zoneIndex);
}

void DaemonRgbDevice::applyLocalAllOff()
{
    const RgbEffect offEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0);
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        setZoneEffect(zoneIndex, offEffect);
        mutableZones()[zoneIndex].setColor(RgbColor(0, 0, 0));
        emit zoneChanged(zoneIndex);
    }
}

BackendCapabilities DaemonRgbDevice::capabilities() const
{
    return m_capabilities;
}

PermissionResult DaemonRgbDevice::checkRuntimePermission(BackendCapability capability) const
{
    const QString key = backendCapabilityToString(capability);
    if (m_permissions.contains(key)) {
        PermissionResult permission = m_permissions.value(key);
        if ((capability == BackendCapability::ZoneColorWrite || capability == BackendCapability::ZoneEffectWrite)
            && m_writeConfirmed
            && permission.status == PermissionStatus::RequiresConfirmation) {
            return {PermissionStatus::Granted, QStringLiteral("Daemon confirmed hardware writes for this session.")};
        }
        return permission;
    }

    if (!m_capabilities.testFlag(capability)) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("%1 does not support %2.").arg(name(), backendCapabilityToString(capability)),
        };
    }

    return {
        PermissionStatus::Denied,
        QStringLiteral("Daemon did not provide a permission snapshot for %1.").arg(key),
    };
}

} // namespace lumacore
