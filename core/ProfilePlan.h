#pragma once

#include "core/RgbDevice.h"
#include "core/RgbEffect.h"
#include "core/RgbZone.h"

#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace lumacore {

enum class ProfileApplyStepKind {
    Skip,
    Target,
};

enum class ProfileApplySkipReason {
    MissingDevice,
    MissingZone,
    InvalidZone,
    UnsupportedEffect,
};

struct ProfileDeviceRef {
    int index {-1};
    const RgbDevice* device {nullptr};
};

struct ProfileApplyTarget {
    int deviceIndex {-1};
    int zoneIndex {-1};
    QString deviceName;
    QString zoneName;
    int ledCount {1};
    RgbEffect effect;
};

struct ProfileApplyStep {
    ProfileApplyStepKind kind {ProfileApplyStepKind::Skip};
    ProfileApplySkipReason skipReason {ProfileApplySkipReason::InvalidZone};
    int skippedZoneCount {1};
    QString detail;
    ProfileApplyTarget target;
};

struct ProfileApplySummary {
    QString profileName;
    int totalZones {0};
    int appliedZones {0};
    int skippedMissingDeviceZones {0};
    int skippedMissingZones {0};
    int skippedInvalidZones {0};
    int skippedUnsupportedZones {0};
    int failedZones {0};
    QStringList details;
};

struct ProfileApplyPlan {
    ProfileApplySummary summary;
    QVector<ProfileApplyStep> steps;
};

[[nodiscard]] int storedProfileEffectType(const QJsonObject& zoneObject);
[[nodiscard]] bool storedProfileZoneEffect(const QJsonObject& zoneObject, RgbEffect* effect, int* effectType = nullptr);
[[nodiscard]] int matchedProfileZoneIndex(const RgbDevice& device, const QJsonObject& zoneObject);
[[nodiscard]] RgbEffect normalizedProfileEffect(const RgbDevice& device, int zoneIndex, const RgbEffect& effect);
[[nodiscard]] QString profileEffectSummary(const RgbEffect& effect);
[[nodiscard]] QVariantMap profileEffectPreview(const RgbEffect& effect);
[[nodiscard]] QStringList profilePreviewChanges(
    const RgbZone& currentZone,
    const RgbEffect& currentEffect,
    const RgbEffect& targetEffect,
    int targetLedCount
);
[[nodiscard]] QVariantMap makeProfilePreviewItem(
    QString status,
    QString statusText,
    QString deviceName,
    QString zoneName,
    QString reason = {}
);
[[nodiscard]] ProfileApplyPlan buildProfileApplyPlan(
    const QString& normalizedProfileName,
    const QJsonObject& profile,
    const QVector<ProfileDeviceRef>& devices
);
void appendProfileApplySkip(ProfileApplySummary* summary, ProfileApplySkipReason reason, int zoneCount, const QString& detail);
[[nodiscard]] QVariantMap profileApplyReport(const ProfileApplySummary& summary);

} // namespace lumacore
