// SPDX-License-Identifier: GPL-2.0-or-later

// Coverage for global/group lighting operations over heterogeneous devices.
// AppControllerTest already exercises the happy path with an all-supporting
// mock device, so every global apply there reports applied == total. This test
// fills the gaps the README calls out: per-zone and global-target effect,
// speed, and brightness capability gating, and partial-result reporting when
// some zones do not support the effect or a device's writes are unconfirmed.

#include "core/DeviceManager.h"
#include "core/RgbDevice.h"
#include "ui/AppController.h"

#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

namespace {

using namespace lumacore;

constexpr int kStatic = static_cast<int>(RgbEffectType::Static);
constexpr int kRainbow = static_cast<int>(RgbEffectType::Rainbow);
constexpr int kBreathing = static_cast<int>(RgbEffectType::Breathing);

bool require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        qCritical().noquote() << message;
    }
    return condition;
}

// A writable device whose zones disagree about what they support: the fixed
// header handles only static color, while the addressable header also handles
// rainbow. Speed exists only on the addressable rainbow; brightness exists only
// on static. This is what makes a global effect apply genuinely partial.
class PartialSupportDevice final : public RgbDevice
{
public:
    explicit PartialSupportDevice(QString id)
        : RgbDevice(std::move(id), QStringLiteral("Partial Support Device"), QStringLiteral("LumaCore"), RgbDeviceType::Controller)
    {
        setLikelyRgbController(true);
        mutableZones().append(RgbZone(QStringLiteral("Fixed Header"), RgbZoneType::Motherboard, 1, RgbColor(10, 20, 30)));
        mutableZones().append(RgbZone(QStringLiteral("Addressable Header"), RgbZoneType::AddressableHeader, 8, RgbColor(40, 50, 60)));
        mutableZones()[0].setEffect(RgbEffect(RgbEffectType::Static, RgbColor(10, 20, 30)));
        mutableZones()[1].setEffect(RgbEffect(RgbEffectType::Static, RgbColor(40, 50, 60)));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override
    {
        if (zoneIndex < 0 || zoneIndex >= mutableZones().size()) {
            return false;
        }
        mutableZones()[zoneIndex].setColor(color);
        return true;
    }

    [[nodiscard]] bool applyAllOff() override
    {
        for (int zoneIndex = 0; zoneIndex < mutableZones().size(); ++zoneIndex) {
            setZoneEffect(zoneIndex, RgbEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0));
        }
        return true;
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {PermissionStatus::Granted, {}};
        }
        return {PermissionStatus::Denied, QStringLiteral("Partial support device does not expose this capability.")};
    }

    [[nodiscard]] bool supportsEffect(int effectType) const override
    {
        return effectType == kStatic || effectType == kRainbow;
    }

    [[nodiscard]] bool supportsZoneEffect(int zoneIndex, int effectType) const override
    {
        if (zoneIndex == 0) {
            return effectType == kStatic;
        }
        if (zoneIndex == 1) {
            return effectType == kStatic || effectType == kRainbow;
        }
        return false;
    }

    [[nodiscard]] bool supportsZoneEffectSpeed(int zoneIndex, int effectType) const override
    {
        return zoneIndex == 1 && effectType == kRainbow;
    }

    [[nodiscard]] bool supportsZoneEffectBrightness(int zoneIndex, int effectType) const override
    {
        return supportsZoneEffect(zoneIndex, effectType) && effectType == kStatic;
    }
};

// A fully write-capable device that requires per-session confirmation before any
// write, used to exercise the confirmation skip in global effect and All Off.
class ConfirmationDevice final : public RgbDevice
{
public:
    explicit ConfirmationDevice(QString id)
        : RgbDevice(std::move(id), QStringLiteral("Confirmation Device"), QStringLiteral("LumaCore"), RgbDeviceType::Controller)
    {
        setLikelyRgbController(true);
        mutableZones().append(RgbZone(QStringLiteral("Effect Zone"), RgbZoneType::AddressableHeader, 4, RgbColor(0, 0, 0)));
        mutableZones()[0].setEffect(RgbEffect(RgbEffectType::Static, RgbColor(0, 0, 0)));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override
    {
        if (zoneIndex < 0 || zoneIndex >= mutableZones().size()) {
            return false;
        }
        mutableZones()[zoneIndex].setColor(color);
        return true;
    }

    [[nodiscard]] bool applyAllOff() override
    {
        for (int zoneIndex = 0; zoneIndex < mutableZones().size(); ++zoneIndex) {
            setZoneEffect(zoneIndex, RgbEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0));
        }
        return true;
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability capability) const override
    {
        if (capability == BackendCapability::DiscoveryRead) {
            return {PermissionStatus::Granted, {}};
        }
        return {PermissionStatus::RequiresConfirmation, QStringLiteral("Confirm writes before applying.")};
    }
};

bool firstDetailContains(const QVariantMap& result, const QString& needle)
{
    const QVariantList details = result.value(QStringLiteral("details")).toList();
    return !details.isEmpty() && details.first().toString().contains(needle);
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    QTemporaryDir settingsDirectory;
    QTemporaryDir profileDirectory;
    if (!require(settingsDirectory.isValid(), "temporary settings directory should be available")
        || !require(profileDirectory.isValid(), "temporary profile directory should be available")) {
        return 1;
    }
    QCoreApplication::setOrganizationName(QStringLiteral("LumaCoreTests"));
    QCoreApplication::setApplicationName(QStringLiteral("GlobalOperationReportTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    // Capability gating and a partial global effect on a single mixed-support device.
    {
        DeviceManager manager(nullptr, profileDirectory.filePath(QStringLiteral("partial-profiles")));
        manager.setDryRunEnabled(false);
        std::vector<std::unique_ptr<RgbDevice>> devices;
        devices.push_back(std::make_unique<PartialSupportDevice>(QStringLiteral("partial-device")));
        manager.replaceDevices(std::move(devices));
        AppController controller(&manager);

        if (!require(controller.deviceSupportsEffect(0, kStatic), "static should be supported at device level")
            || !require(controller.deviceSupportsEffect(0, kRainbow), "rainbow should be supported through the addressable zone")
            || !require(!controller.deviceSupportsEffect(0, kBreathing), "breathing should be unsupported")
            || !require(!controller.zoneSupportsEffect(0, 0, kRainbow), "the fixed zone should reject rainbow")
            || !require(controller.zoneSupportsEffect(0, 1, kRainbow), "the addressable zone should allow rainbow")
            || !require(controller.zoneSupportsEffectSpeed(0, 1, kRainbow), "the addressable rainbow should expose speed")
            || !require(!controller.zoneSupportsEffectSpeed(0, 0, kRainbow), "the fixed zone should expose no rainbow speed")
            || !require(!controller.zoneSupportsEffectSpeed(0, 1, kStatic), "static should never expose speed")
            || !require(controller.zoneSupportsEffectBrightness(0, 0, kStatic), "the fixed static zone should expose brightness")
            || !require(!controller.zoneSupportsEffectBrightness(0, 1, kRainbow), "the addressable rainbow should expose no brightness")) {
            return 1;
        }

        if (!require(controller.globalTargetSupportsEffect(QString(), kStatic), "global target should support static")
            || !require(controller.globalTargetSupportsEffect(QString(), kRainbow), "global target should support rainbow")
            || !require(!controller.globalTargetSupportsEffect(QString(), kBreathing), "global target should reject breathing")
            || !require(controller.globalTargetSupportsEffectSpeed(QString(), kRainbow), "global target should expose rainbow speed")
            || !require(!controller.globalTargetSupportsEffectSpeed(QString(), kStatic), "global target should expose no static speed")
            || !require(controller.globalTargetSupportsEffectBrightness(QString(), kStatic), "global target should expose static brightness")
            || !require(!controller.globalTargetSupportsEffectBrightness(QString(), kRainbow), "global target should expose no rainbow brightness")) {
            return 1;
        }

        QVariantMap rainbowResult;
        QObject::connect(&controller, &AppController::globalOperationFinished, &controller,
            [&rainbowResult](const QVariantMap& result) { rainbowResult = result; });

        const QColor color(QStringLiteral("#3366CC"));
        if (!require(controller.applyEffectGlobally(kRainbow, color, 2.5, 40), "a partial global effect should still start")
            || !require(rainbowResult.value(QStringLiteral("total")).toInt() == 2, "both zones should be counted in the total")
            || !require(rainbowResult.value(QStringLiteral("applied")).toInt() == 1, "only the supporting zone should apply")
            || !require(rainbowResult.value(QStringLiteral("skipped")).toInt() == 1, "the unsupported zone should be skipped")
            || !require(rainbowResult.value(QStringLiteral("failed")).toInt() == 0, "no zone should fail")
            || !require(rainbowResult.value(QStringLiteral("partial")).toBool(), "a mixed apply should report partial")
            || !require(rainbowResult.value(QStringLiteral("targetKind")).toString() == QStringLiteral("all"), "the target kind should be all")
            || !require(firstDetailContains(rainbowResult, QStringLiteral("effect is not supported")), "the skip detail should name the reason")) {
            return 1;
        }

        if (!require(controller.zoneEffectType(0, 1) == kRainbow, "the addressable zone should become rainbow")
            || !require(qFuzzyCompare(controller.zoneEffectSpeed(0, 1), 2.5), "a supported speed should be kept")
            || !require(controller.zoneEffectBrightness(0, 1) == 100, "an unsupported brightness should be coerced to full")
            || !require(controller.zoneEffectType(0, 0) == kStatic, "the skipped zone should keep its static effect")) {
            return 1;
        }

        QVariantMap breathingResult;
        QObject::connect(&controller, &AppController::globalOperationFinished, &controller,
            [&breathingResult](const QVariantMap& result) { breathingResult = result; });
        if (!require(controller.applyEffectGlobally(kBreathing, color, 1.0, 50), "an unsupported global effect should still run")
            || !require(breathingResult.value(QStringLiteral("applied")).toInt() == 0, "no zone should apply an unsupported effect")
            || !require(breathingResult.value(QStringLiteral("skipped")).toInt() == 2, "every zone should be skipped")
            || !require(!breathingResult.value(QStringLiteral("partial")).toBool(), "an all-skipped apply should not be partial")) {
            return 1;
        }
    }

    // Confirmation gating across a mixed device set for both global effect and All Off.
    {
        DeviceManager manager(nullptr, profileDirectory.filePath(QStringLiteral("confirm-profiles")));
        manager.setDryRunEnabled(false);
        std::vector<std::unique_ptr<RgbDevice>> devices;
        devices.push_back(std::make_unique<PartialSupportDevice>(QStringLiteral("granted-device")));
        devices.push_back(std::make_unique<ConfirmationDevice>(QStringLiteral("confirm-device")));
        manager.replaceDevices(std::move(devices));
        AppController controller(&manager);

        if (!require(controller.deviceRequiresConfirmation(1), "the confirmation device should require confirmation")
            || !require(!controller.deviceWriteConfirmed(1), "the confirmation device should start unconfirmed")) {
            return 1;
        }

        QVariantMap mixedResult;
        QObject::connect(&controller, &AppController::globalOperationFinished, &controller,
            [&mixedResult](const QVariantMap& result) { mixedResult = result; });

        const QColor color(QStringLiteral("#88AA22"));
        if (!require(controller.applyEffectGlobally(kStatic, color, 1.0, 80), "a mixed global effect should start")
            || !require(mixedResult.value(QStringLiteral("total")).toInt() == 3, "all writable zones should be counted")
            || !require(mixedResult.value(QStringLiteral("applied")).toInt() == 2, "only the confirmed device's zones should apply")
            || !require(mixedResult.value(QStringLiteral("skipped")).toInt() == 1, "the unconfirmed zone should be skipped")
            || !require(mixedResult.value(QStringLiteral("partial")).toBool(), "the mix should report partial")
            || !require(firstDetailContains(mixedResult, QStringLiteral("hardware writes are not confirmed")), "the skip detail should name confirmation")) {
            return 1;
        }

        // Confirmation is daemon-owned, so the controller's confirm method is a
        // daemon round-trip; drive the session confirmation through the manager
        // and read it back through the controller the global path consults.
        if (!require(manager.confirmDeviceWrites(1), "confirming writes should succeed")
            || !require(controller.deviceWriteConfirmed(1), "the device should report confirmed writes")) {
            return 1;
        }
        QVariantMap confirmedResult;
        QObject::connect(&controller, &AppController::globalOperationFinished, &controller,
            [&confirmedResult](const QVariantMap& result) { confirmedResult = result; });
        if (!require(controller.applyEffectGlobally(kStatic, color, 1.0, 80), "a confirmed global effect should start")
            || !require(confirmedResult.value(QStringLiteral("applied")).toInt() == 3, "all zones should apply once confirmed")
            || !require(confirmedResult.value(QStringLiteral("skipped")).toInt() == 0, "nothing should be skipped once confirmed")
            || !require(!confirmedResult.value(QStringLiteral("partial")).toBool(), "a full apply should not be partial")) {
            return 1;
        }

        if (!require(manager.revokeDeviceWrites(1), "revoking writes should succeed")) {
            return 1;
        }
        QVariantMap allOffResult;
        QObject::connect(&controller, &AppController::globalOperationFinished, &controller,
            [&allOffResult](const QVariantMap& result) { allOffResult = result; });
        if (!require(controller.allOffAllDevices(), "a mixed All Off should start")
            || !require(allOffResult.value(QStringLiteral("operation")).toString() == QStringLiteral("All Off"), "the operation should be All Off")
            || !require(allOffResult.value(QStringLiteral("total")).toInt() == 2, "All Off should count writable devices, not zones")
            || !require(allOffResult.value(QStringLiteral("applied")).toInt() == 1, "only the granted device should turn off")
            || !require(allOffResult.value(QStringLiteral("skipped")).toInt() == 1, "the unconfirmed device should be skipped")
            || !require(allOffResult.value(QStringLiteral("partial")).toBool(), "the mixed All Off should report partial")
            || !require(firstDetailContains(allOffResult, QStringLiteral("hardware writes are not confirmed")), "the All Off skip detail should name confirmation")) {
            return 1;
        }

        if (!require(manager.confirmDeviceWrites(1), "re-confirming writes should succeed")) {
            return 1;
        }
        QVariantMap allOffConfirmed;
        QObject::connect(&controller, &AppController::globalOperationFinished, &controller,
            [&allOffConfirmed](const QVariantMap& result) { allOffConfirmed = result; });
        if (!require(controller.allOffAllDevices(), "a confirmed All Off should start")
            || !require(allOffConfirmed.value(QStringLiteral("applied")).toInt() == 2, "both devices should turn off once confirmed")
            || !require(!allOffConfirmed.value(QStringLiteral("partial")).toBool(), "a full All Off should not be partial")) {
            return 1;
        }
    }

    return 0;
}
