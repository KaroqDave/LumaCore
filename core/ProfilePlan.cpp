#include "core/ProfilePlan.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QtGlobal>

#include <utility>

namespace lumacore {

int storedProfileEffectType(const QJsonObject& zoneObject)
{
    const QJsonObject effectObject = zoneObject.value(QStringLiteral("effect")).toObject();
    if (effectObject.isEmpty()) {
        return static_cast<int>(RgbEffectType::Static);
    }

    const QString effectName = effectObject.value(QStringLiteral("type")).toString();
    for (const RgbEffectType effectType : allRgbEffectTypes()) {
        if (effectName.compare(rgbEffectTypeToString(effectType), Qt::CaseInsensitive) == 0) {
            return static_cast<int>(effectType);
        }
    }
    return -1;
}

bool storedProfileZoneEffect(const QJsonObject& zoneObject, RgbEffect* effect, int* effectType)
{
    const int typeValue = storedProfileEffectType(zoneObject);
    if (effectType != nullptr) {
        *effectType = typeValue;
    }
    if (typeValue < 0) {
        return false;
    }

    if (!zoneObject.contains(QStringLiteral("effect"))) {
        bool colorOk = false;
        const RgbColor color = RgbColor::fromHexString(zoneObject.value(QStringLiteral("color")).toString(), &colorOk);
        if (!colorOk) {
            return false;
        }
        if (effect != nullptr) {
            *effect = RgbEffect(RgbEffectType::Static, color);
        }
        return true;
    }

    const QJsonValue effectValue = zoneObject.value(QStringLiteral("effect"));
    if (!effectValue.isObject()) {
        return false;
    }

    const QJsonObject effectObject = effectValue.toObject();
    bool colorOk = false;
    const RgbColor color = RgbColor::fromHexString(effectObject.value(QStringLiteral("color")).toString(), &colorOk);
    if (!colorOk) {
        return false;
    }

    if (effect != nullptr) {
        *effect = RgbEffect(
            static_cast<RgbEffectType>(typeValue),
            color,
            effectObject.value(QStringLiteral("speed")).toDouble(1.0),
            effectObject.value(QStringLiteral("brightness")).toInt(100)
        );
    }
    return true;
}

int matchedProfileZoneIndex(const RgbDevice& device, const QJsonObject& zoneObject)
{
    const QString zoneName = zoneObject.value(QStringLiteral("name")).toString();
    for (int zoneIndex = 0; zoneIndex < device.zones().size(); ++zoneIndex) {
        if (device.zones().at(zoneIndex).name() == zoneName) {
            return zoneIndex;
        }
    }

    const int storedZoneIndex = zoneObject.value(QStringLiteral("index")).toInt(-1);
    return storedZoneIndex >= 0 && storedZoneIndex < device.zones().size()
        ? storedZoneIndex
        : -1;
}

RgbEffect normalizedProfileEffect(const RgbDevice& device, int zoneIndex, const RgbEffect& effect)
{
    RgbEffect normalized = effect;
    const int effectType = static_cast<int>(effect.type());
    if (!device.supportsZoneEffectSpeed(zoneIndex, effectType)) {
        normalized.setSpeed(1.0);
    }
    if (!device.supportsZoneEffectBrightness(zoneIndex, effectType)) {
        normalized.setBrightness(100);
    }
    return normalized;
}

QString profileEffectSummary(const RgbEffect& effect)
{
    const QString base = QStringLiteral("%1 %2, brightness %3")
        .arg(rgbEffectTypeToString(effect.type()), effect.color().toHexString())
        .arg(effect.brightness());
    if (!effect.isAnimated()) {
        return base;
    }

    return QStringLiteral("%1, speed %2")
        .arg(base)
        .arg(effect.speed(), 0, 'f', 1);
}

QVariantMap profileEffectPreview(const RgbEffect& effect)
{
    return {
        {QStringLiteral("type"), rgbEffectTypeToString(effect.type())},
        {QStringLiteral("color"), effect.color().toHexString()},
        {QStringLiteral("brightness"), effect.brightness()},
        {QStringLiteral("speed"), effect.speed()},
        {QStringLiteral("summary"), profileEffectSummary(effect)},
    };
}

QStringList profilePreviewChanges(
    const RgbZone& currentZone,
    const RgbEffect& currentEffect,
    const RgbEffect& targetEffect,
    int targetLedCount
)
{
    QStringList changes;
    if (currentEffect.type() != targetEffect.type()) {
        changes.append(
            QStringLiteral("Effect: %1 -> %2")
                .arg(rgbEffectTypeToString(currentEffect.type()), rgbEffectTypeToString(targetEffect.type()))
        );
    }
    if (currentEffect.color() != targetEffect.color()) {
        changes.append(
            QStringLiteral("Color: %1 -> %2")
                .arg(currentEffect.color().toHexString(), targetEffect.color().toHexString())
        );
    }
    if (currentEffect.brightness() != targetEffect.brightness()) {
        changes.append(
            QStringLiteral("Brightness: %1 -> %2")
                .arg(currentEffect.brightness())
                .arg(targetEffect.brightness())
        );
    }
    if (!qFuzzyCompare(currentEffect.speed(), targetEffect.speed())) {
        changes.append(
            QStringLiteral("Speed: %1 -> %2")
                .arg(currentEffect.speed(), 0, 'f', 1)
                .arg(targetEffect.speed(), 0, 'f', 1)
        );
    }
    if (currentZone.ledCount() != targetLedCount) {
        changes.append(
            QStringLiteral("LED count: %1 -> %2")
                .arg(currentZone.ledCount())
                .arg(targetLedCount)
        );
    }
    return changes;
}

QVariantMap makeProfilePreviewItem(
    QString status,
    QString statusText,
    QString deviceName,
    QString zoneName,
    QString reason
)
{
    return {
        {QStringLiteral("status"), std::move(status)},
        {QStringLiteral("statusText"), std::move(statusText)},
        {QStringLiteral("deviceName"), std::move(deviceName)},
        {QStringLiteral("zoneName"), std::move(zoneName)},
        {QStringLiteral("reason"), std::move(reason)},
        {QStringLiteral("changed"), false},
        {QStringLiteral("changes"), QStringList {}},
        {QStringLiteral("changeSummary"), QString()},
        {QStringLiteral("currentSummary"), QString()},
        {QStringLiteral("targetSummary"), QString()},
        {QStringLiteral("currentLedCount"), 0},
        {QStringLiteral("targetLedCount"), 0},
        {QStringLiteral("currentEffect"), QVariantMap {}},
        {QStringLiteral("targetEffect"), QVariantMap {}},
    };
}

namespace {

const RgbDevice* deviceById(const QVector<ProfileDeviceRef>& devices, const QString& deviceId, int* deviceIndex)
{
    for (const ProfileDeviceRef& ref : devices) {
        if (ref.device != nullptr && ref.device->id() == deviceId) {
            if (deviceIndex != nullptr) {
                *deviceIndex = ref.index;
            }
            return ref.device;
        }
    }
    return nullptr;
}

} // namespace

ProfileApplyPlan buildProfileApplyPlan(
    const QString& normalizedProfileName,
    const QJsonObject& profile,
    const QVector<ProfileDeviceRef>& devices
)
{
    ProfileApplyPlan plan;
    plan.summary.profileName = normalizedProfileName;

    const QJsonArray devicesJson = profile.value(QStringLiteral("devices")).toArray();
    for (const QJsonValue& deviceValue : devicesJson) {
        const QJsonObject deviceObject = deviceValue.toObject();
        const QString deviceId = deviceObject.value(QStringLiteral("id")).toString();
        const QString storedDeviceName = deviceObject.value(QStringLiteral("name")).toString(deviceId);
        const QJsonArray zonesJson = deviceObject.value(QStringLiteral("zones")).toArray();
        const int storedZoneCount = static_cast<int>(zonesJson.size());
        plan.summary.totalZones += storedZoneCount;

        int deviceIndex = -1;
        const RgbDevice* device = deviceById(devices, deviceId, &deviceIndex);
        if (device == nullptr) {
            plan.steps.append(ProfileApplyStep {
                ProfileApplyStepKind::Skip,
                ProfileApplySkipReason::MissingDevice,
                storedZoneCount,
                QStringLiteral("Skipped missing device: %1 (%2 zone(s))").arg(storedDeviceName).arg(storedZoneCount),
                {},
            });
            continue;
        }

        for (const QJsonValue& zoneValue : zonesJson) {
            const QJsonObject zoneObject = zoneValue.toObject();
            const QString zoneName = zoneObject.value(QStringLiteral("name")).toString();
            int effectType = -1;
            RgbEffect effect;
            if (!storedProfileZoneEffect(zoneObject, &effect, &effectType)) {
                plan.steps.append(ProfileApplyStep {
                    ProfileApplyStepKind::Skip,
                    ProfileApplySkipReason::InvalidZone,
                    1,
                    QStringLiteral("Skipped invalid zone: %1 / %2").arg(storedDeviceName, zoneName),
                    {},
                });
                continue;
            }

            const int matchedZone = matchedProfileZoneIndex(*device, zoneObject);
            if (matchedZone < 0) {
                plan.steps.append(ProfileApplyStep {
                    ProfileApplyStepKind::Skip,
                    ProfileApplySkipReason::MissingZone,
                    1,
                    QStringLiteral("Skipped missing zone: %1 / %2").arg(storedDeviceName, zoneName),
                    {},
                });
                continue;
            }
            if (!device->supportsZoneEffect(matchedZone, effectType)) {
                plan.steps.append(ProfileApplyStep {
                    ProfileApplyStepKind::Skip,
                    ProfileApplySkipReason::UnsupportedEffect,
                    1,
                    QStringLiteral("Skipped unsupported effect: %1 / %2").arg(storedDeviceName, zoneName),
                    {},
                });
                continue;
            }

            plan.steps.append(ProfileApplyStep {
                ProfileApplyStepKind::Target,
                ProfileApplySkipReason::InvalidZone,
                0,
                {},
                ProfileApplyTarget {
                    deviceIndex,
                    matchedZone,
                    storedDeviceName,
                    zoneName,
                    qMax(1, zoneObject.value(QStringLiteral("ledCount")).toInt(device->zones().at(matchedZone).ledCount())),
                    normalizedProfileEffect(*device, matchedZone, effect),
                },
            });
        }
    }

    return plan;
}

void appendProfileApplySkip(ProfileApplySummary* summary, ProfileApplySkipReason reason, int zoneCount, const QString& detail)
{
    if (summary == nullptr) {
        return;
    }

    switch (reason) {
    case ProfileApplySkipReason::MissingDevice:
        summary->skippedMissingDeviceZones += zoneCount;
        break;
    case ProfileApplySkipReason::MissingZone:
        summary->skippedMissingZones += zoneCount;
        break;
    case ProfileApplySkipReason::InvalidZone:
        summary->skippedInvalidZones += zoneCount;
        break;
    case ProfileApplySkipReason::UnsupportedEffect:
        summary->skippedUnsupportedZones += zoneCount;
        break;
    }
    summary->details.append(detail);
}

QVariantMap profileApplyReport(const ProfileApplySummary& summary)
{
    const int skippedZones = summary.skippedMissingDeviceZones
        + summary.skippedMissingZones
        + summary.skippedInvalidZones
        + summary.skippedUnsupportedZones
        + summary.failedZones;
    const bool success = summary.appliedZones > 0;
    const QString text = success
        ? QStringLiteral("Applied %1 of %2 zone(s); skipped or failed %3.")
              .arg(summary.appliedZones)
              .arg(summary.totalZones)
              .arg(skippedZones)
        : QStringLiteral("Applied 0 of %1 zone(s).").arg(summary.totalZones);

    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("partial"), success && skippedZones > 0},
        {QStringLiteral("profileName"), summary.profileName},
        {QStringLiteral("totalZones"), summary.totalZones},
        {QStringLiteral("appliedZones"), summary.appliedZones},
        {QStringLiteral("skippedZones"), skippedZones},
        {QStringLiteral("missingDeviceZones"), summary.skippedMissingDeviceZones},
        {QStringLiteral("missingZones"), summary.skippedMissingZones},
        {QStringLiteral("invalidZones"), summary.skippedInvalidZones},
        {QStringLiteral("unsupportedZones"), summary.skippedUnsupportedZones},
        {QStringLiteral("failedZones"), summary.failedZones},
        {QStringLiteral("summary"), text},
        {
            QStringLiteral("error"),
            success ? QString() : QStringLiteral("Profile did not match any available device zones."),
        },
        {QStringLiteral("details"), summary.details},
    };
}

} // namespace lumacore
