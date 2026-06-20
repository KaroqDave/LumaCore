#include "ipc/DaemonProtocol.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QtGlobal>

namespace lumacore {

QString defaultDaemonSocketPath()
{
#ifdef Q_OS_WIN
    QString userScope = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (userScope.isEmpty()) {
        userScope = QDir::homePath();
    }
    const QByteArray userHash = QCryptographicHash::hash(
        QDir::cleanPath(userScope).toLower().toUtf8(),
        QCryptographicHash::Sha256
    ).toHex().left(12);
    return QStringLiteral("lumacore-daemon-v1-%1").arg(QString::fromLatin1(userHash));
#else
    return QStringLiteral("/run/lumacore/lumacore.sock");
#endif
}

QByteArray encodeDaemonMessage(const QJsonObject& message)
{
    QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    payload.append('\n');
    return payload;
}

QJsonObject makeDaemonRequest(quint64 id, const QString& method, const QJsonObject& params)
{
    return {
        {QStringLiteral("version"), kDaemonProtocolVersion},
        {QStringLiteral("id"), QString::number(id)},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params},
    };
}

QJsonObject makeDaemonResult(quint64 id, const QJsonObject& result)
{
    return {
        {QStringLiteral("version"), kDaemonProtocolVersion},
        {QStringLiteral("id"), QString::number(id)},
        {QStringLiteral("ok"), true},
        {QStringLiteral("result"), result},
    };
}

QJsonObject makeDaemonError(quint64 id, const QString& message)
{
    return {
        {QStringLiteral("version"), kDaemonProtocolVersion},
        {QStringLiteral("id"), QString::number(id)},
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), message},
    };
}

QString permissionStatusToString(PermissionStatus status)
{
    switch (status) {
    case PermissionStatus::Granted:
        return QStringLiteral("granted");
    case PermissionStatus::Denied:
        return QStringLiteral("denied");
    case PermissionStatus::RequiresConfirmation:
        return QStringLiteral("requiresConfirmation");
    case PermissionStatus::NotApplicable:
        return QStringLiteral("notApplicable");
    }

    return QStringLiteral("denied");
}

PermissionStatus permissionStatusFromString(const QString& value)
{
    if (value.compare(QStringLiteral("granted"), Qt::CaseInsensitive) == 0) {
        return PermissionStatus::Granted;
    }

    if (value.compare(QStringLiteral("requiresConfirmation"), Qt::CaseInsensitive) == 0) {
        return PermissionStatus::RequiresConfirmation;
    }

    if (value.compare(QStringLiteral("notApplicable"), Qt::CaseInsensitive) == 0) {
        return PermissionStatus::NotApplicable;
    }

    return PermissionStatus::Denied;
}

QJsonArray capabilitiesToJson(BackendCapabilities capabilities)
{
    QJsonArray values;
    if (capabilities.testFlag(BackendCapability::DiscoveryRead)) {
        values.append(backendCapabilityToString(BackendCapability::DiscoveryRead));
    }
    if (capabilities.testFlag(BackendCapability::ZoneColorWrite)) {
        values.append(backendCapabilityToString(BackendCapability::ZoneColorWrite));
    }
    if (capabilities.testFlag(BackendCapability::ZoneEffectWrite)) {
        values.append(backendCapabilityToString(BackendCapability::ZoneEffectWrite));
    }
    return values;
}

BackendCapabilities capabilitiesFromJson(const QJsonArray& values)
{
    BackendCapabilities capabilities = BackendCapability::None;
    for (const QJsonValue& value : values) {
        const QString capability = value.toString();
        if (capability == backendCapabilityToString(BackendCapability::DiscoveryRead)) {
            capabilities |= BackendCapability::DiscoveryRead;
        } else if (capability == backendCapabilityToString(BackendCapability::ZoneColorWrite)) {
            capabilities |= BackendCapability::ZoneColorWrite;
        } else if (capability == backendCapabilityToString(BackendCapability::ZoneEffectWrite)) {
            capabilities |= BackendCapability::ZoneEffectWrite;
        }
    }
    return capabilities;
}

QJsonObject permissionResultToJson(const PermissionResult& result)
{
    return {
        {QStringLiteral("status"), permissionStatusToString(result.status)},
        {QStringLiteral("reason"), result.reason},
    };
}

PermissionResult permissionResultFromJson(const QJsonObject& object)
{
    return {
        permissionStatusFromString(object.value(QStringLiteral("status")).toString()),
        object.value(QStringLiteral("reason")).toString(),
    };
}

QJsonObject backendDescriptorToJson(const BackendDescriptor& descriptor)
{
    return {
        {QStringLiteral("id"), descriptor.id},
        {QStringLiteral("displayName"), descriptor.displayName},
        {QStringLiteral("description"), descriptor.description},
        {QStringLiteral("capabilities"), capabilitiesToJson(descriptor.capabilities)},
    };
}

BackendDescriptor backendDescriptorFromJson(const QJsonObject& object)
{
    return {
        object.value(QStringLiteral("id")).toString(),
        object.value(QStringLiteral("displayName")).toString(),
        object.value(QStringLiteral("description")).toString(),
        capabilitiesFromJson(object.value(QStringLiteral("capabilities")).toArray()),
    };
}

QJsonObject zoneToJson(const RgbZone& zone)
{
    return {
        {QStringLiteral("name"), zone.name()},
        {QStringLiteral("type"), zone.typeName()},
        {QStringLiteral("ledCount"), zone.ledCount()},
        {QStringLiteral("color"), zone.currentColor().toHexString()},
        {QStringLiteral("effect"), zone.effect().toJson()},
    };
}

bool isAnimatedEffectType(RgbEffectType type)
{
    return type != RgbEffectType::Static;
}

QJsonArray effectSupportToJson(const RgbDevice& device)
{
    const BackendCapabilities capabilities = device.capabilities();
    const bool supportsStatic = capabilities.testFlag(BackendCapability::ZoneColorWrite);
    const bool supportsAnimated = capabilities.testFlag(BackendCapability::ZoneEffectWrite);
    const bool isAsusAura = device.backendId() == QStringLiteral("asus-aura-hid");

    QJsonArray effects;
    for (const RgbEffectType type : {
             RgbEffectType::Static,
             RgbEffectType::Rainbow,
             RgbEffectType::Breathing,
             RgbEffectType::ColorCycle,
         }) {
        const bool animated = isAnimatedEffectType(type);
        const bool supported = animated ? supportsAnimated : supportsStatic;
        bool speed = supported && animated;
        bool brightness = supported;

        if (isAsusAura) {
            speed = false;
            brightness = type == RgbEffectType::Static || type == RgbEffectType::Breathing;
        }

        effects.append(QJsonObject {
            {QStringLiteral("effectType"), static_cast<int>(type)},
            {QStringLiteral("name"), rgbEffectTypeToString(type)},
            {QStringLiteral("supported"), supported},
            {QStringLiteral("speed"), speed},
            {QStringLiteral("brightness"), brightness},
        });
    }
    return effects;
}

QJsonObject permissionResultsToJson(const RgbDevice& device)
{
    return {
        {backendCapabilityToString(BackendCapability::DiscoveryRead),
         permissionResultToJson(device.checkRuntimePermission(BackendCapability::DiscoveryRead))},
        {backendCapabilityToString(BackendCapability::ZoneColorWrite),
         permissionResultToJson(device.checkRuntimePermission(BackendCapability::ZoneColorWrite))},
        {backendCapabilityToString(BackendCapability::ZoneEffectWrite),
         permissionResultToJson(device.checkRuntimePermission(BackendCapability::ZoneEffectWrite))},
    };
}

QJsonObject deviceToJson(const RgbDevice& device, int index, bool writeConfirmed)
{
    QJsonArray zones;
    for (const RgbZone& zone : device.zones()) {
        zones.append(zoneToJson(zone));
    }
    QJsonObject permissions = permissionResultsToJson(device);
    const BackendCapabilities capabilities = device.capabilities();
    PermissionResult colorPermission = permissionResultFromJson(
        permissions.value(backendCapabilityToString(BackendCapability::ZoneColorWrite)).toObject()
    );
    if (writeConfirmed && colorPermission.status == PermissionStatus::RequiresConfirmation) {
        colorPermission = {
            PermissionStatus::Granted,
            QStringLiteral("Hardware writes are confirmed for this daemon session."),
        };
        permissions.insert(
            backendCapabilityToString(BackendCapability::ZoneColorWrite),
            permissionResultToJson(colorPermission)
        );
    }
    PermissionResult effectPermission = permissionResultFromJson(
        permissions.value(backendCapabilityToString(BackendCapability::ZoneEffectWrite)).toObject()
    );
    if (writeConfirmed && effectPermission.status == PermissionStatus::RequiresConfirmation) {
        effectPermission = {
            PermissionStatus::Granted,
            QStringLiteral("Hardware writes are confirmed for this daemon session."),
        };
        permissions.insert(
            backendCapabilityToString(BackendCapability::ZoneEffectWrite),
            permissionResultToJson(effectPermission)
        );
    }

    return {
        {QStringLiteral("index"), index},
        {QStringLiteral("id"), device.id()},
        {QStringLiteral("name"), device.name()},
        {QStringLiteral("vendor"), device.vendor()},
        {QStringLiteral("type"), device.typeName()},
        {QStringLiteral("backendId"), device.backendId()},
        {QStringLiteral("realBackendId"), device.backendId()},
        {QStringLiteral("capabilities"), capabilitiesToJson(capabilities)},
        {QStringLiteral("permission"), permissionResultToJson(colorPermission)},
        {QStringLiteral("permissions"), permissions},
        {QStringLiteral("effectSupport"), effectSupportToJson(device)},
        {QStringLiteral("lastHardwareWriteStatus"), device.lastHardwareWriteStatus()},
        {QStringLiteral("writeConfirmed"), writeConfirmed},
        {QStringLiteral("writeRequiresConfirmation"), colorPermission.status == PermissionStatus::RequiresConfirmation},
        {QStringLiteral("readOnly"), !capabilities.testFlag(BackendCapability::ZoneColorWrite)
             && !capabilities.testFlag(BackendCapability::ZoneEffectWrite)},
        {QStringLiteral("likelyRgbController"), device.likelyRgbController()},
        {QStringLiteral("isRgbController"), device.isRgbController()},
        {QStringLiteral("hasRgbControllerOverride"), device.hasRgbControllerOverride()},
        {QStringLiteral("rgbControllerOverride"), device.rgbControllerOverride()},
        {QStringLiteral("zones"), zones},
    };
}

RgbEffect effectFromJson(const QJsonObject& object)
{
    return RgbEffect::fromJson(object);
}

} // namespace lumacore
