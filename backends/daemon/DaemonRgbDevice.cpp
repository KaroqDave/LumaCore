#include "backends/daemon/DaemonRgbDevice.h"

#include "ipc/DaemonProtocol.h"

#include <QJsonArray>
#include <QJsonObject>

#include <utility>

namespace lumacore {

namespace {

QString deviceString(const QJsonObject& snapshot, const QString& key, const QString& fallback = {})
{
    const QString value = snapshot.value(key).toString();
    return value.isEmpty() ? fallback : value;
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
    , m_capabilities(capabilitiesFromJson(snapshot.value(QStringLiteral("capabilities")).toArray()))
    , m_permission(permissionResultFromJson(snapshot.value(QStringLiteral("permission")).toObject()))
    , m_writeConfirmed(snapshot.value(QStringLiteral("writeConfirmed")).toBool(false))
    , m_writeRequiresConfirmation(snapshot.value(QStringLiteral("writeRequiresConfirmation")).toBool(false))
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

    const QJsonArray effectSupport = snapshot.value(QStringLiteral("effectSupport")).toArray();
    for (const QJsonValue& supportValue : effectSupport) {
        const QJsonObject supportObject = supportValue.toObject();
        m_effectSupport.insert(
            supportObject.value(QStringLiteral("effectType")).toInt(-1),
            {
                supportObject.value(QStringLiteral("supported")).toBool(false),
                supportObject.value(QStringLiteral("speed")).toBool(false),
                supportObject.value(QStringLiteral("brightness")).toBool(false),
            }
        );
    }

    const QJsonArray zones = snapshot.value(QStringLiteral("zones")).toArray();
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
    }
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

    const DaemonCallResult result = m_client->call(QStringLiteral("applyEffect"), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
        {QStringLiteral("zoneIndex"), zoneIndex},
        {QStringLiteral("effect"), effect.toJson()},
    });
    m_lastHardwareWriteStatus = result.result.value(QStringLiteral("hardwareStatus")).toString(
        result.ok ? result.result.value(QStringLiteral("error")).toString() : result.error
    );
    if (!result.ok || !result.result.value(QStringLiteral("success")).toBool(false)) {
        return false;
    }

    setZoneEffect(zoneIndex, effect);
    if (!effect.isAnimated()) {
        const bool colorApplied = setZoneStaticColor(zoneIndex, effect.color());
        Q_UNUSED(colorApplied)
    } else {
        emit zoneChanged(zoneIndex);
    }
    return true;
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

    const DaemonCallResult result = m_client->call(QStringLiteral("allOff"), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
    });
    m_lastHardwareWriteStatus = result.result.value(QStringLiteral("hardwareStatus")).toString(
        result.ok ? result.result.value(QStringLiteral("error")).toString() : result.error
    );
    if (!result.ok || !result.result.value(QStringLiteral("success")).toBool(false)) {
        return false;
    }

    const RgbEffect offEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0);
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        setZoneEffect(zoneIndex, offEffect);
        mutableZones()[zoneIndex].setColor(RgbColor(0, 0, 0));
        emit zoneChanged(zoneIndex);
    }
    return true;
}

bool DaemonRgbDevice::updateZoneMetadata(int zoneIndex, const QString& name, int ledCount)
{
    if (m_client == nullptr || zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }

    const DaemonCallResult result = m_client->call(QStringLiteral("updateZone"), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
        {QStringLiteral("zoneIndex"), zoneIndex},
        {QStringLiteral("name"), name.trimmed()},
        {QStringLiteral("ledCount"), ledCount},
    });
    if (!result.ok || !result.result.value(QStringLiteral("success")).toBool(false)) {
        return false;
    }

    const bool changedName = setZoneName(zoneIndex, name);
    const bool changedLedCount = setZoneLedCount(zoneIndex, ledCount);
    return changedName || changedLedCount;
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

    const DaemonCallResult result = m_client->call(QStringLiteral("previewEffect"), {
        {QStringLiteral("deviceIndex"), m_daemonDeviceIndex},
        {QStringLiteral("zoneIndex"), zoneIndex},
        {QStringLiteral("effect"), effect.toJson()},
    });
    if (!result.ok || !result.result.value(QStringLiteral("success")).toBool(false)) {
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
    return m_writeRequiresConfirmation;
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

void DaemonRgbDevice::setWriteConfirmed(bool confirmed)
{
    m_writeConfirmed = confirmed;
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
