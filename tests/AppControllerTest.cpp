// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockBackend.h"
#include "backends/daemon/DaemonBackend.h"
#include "backends/daemon/DaemonRgbDevice.h"
#include "core/DeviceManager.h"
#include "core/ProfileStore.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"
#include "ui/AppController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <functional>
#include <utility>
#include <vector>
#include <cstdio>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        qCritical().noquote() << message;
    }
    return condition;
}

class StableIdDevice final : public lumacore::RgbDevice
{
public:
    StableIdDevice(QString id, QString zoneName)
        : RgbDevice(
              std::move(id),
              QStringLiteral("Selection Test Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            std::move(zoneName),
            lumacore::RgbZoneType::AddressableHeader,
            1
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        if (zoneIndex < 0 || zoneIndex >= mutableZones().size()) {
            return false;
        }
        mutableZones()[zoneIndex].setColor(color);
        return true;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead | lumacore::BackendCapability::ZoneColorWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }

        return {
            lumacore::PermissionStatus::Denied,
            QStringLiteral("Test device does not support %1.").arg(lumacore::backendCapabilityToString(capability)),
        };
    }
};

class AnimPreviewDevice final : public lumacore::RgbDevice
{
public:
    AnimPreviewDevice()
        : RgbDevice(
              QStringLiteral("anim-preview-device"),
              QStringLiteral("Anim Preview Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Header"),
            lumacore::RgbZoneType::AddressableHeader,
            8
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        if (zoneIndex < 0 || zoneIndex >= mutableZones().size()) {
            return false;
        }
        mutableZones()[zoneIndex].setColor(color);
        return true;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneColorWrite
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported anim preview capability.")};
    }
};

class GroupTestDevice final : public lumacore::RgbDevice
{
public:
    GroupTestDevice(QString id, QString name)
        : RgbDevice(
              std::move(id),
              std::move(name),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        setLikelyRgbController(true);
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Zone"),
            lumacore::RgbZoneType::AddressableHeader,
            4,
            lumacore::RgbColor(0, 0, 0)
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        return applyZoneEffect(zoneIndex, lumacore::RgbEffect(lumacore::RgbEffectType::Static, color));
    }

    [[nodiscard]] bool applyAllOff() override
    {
        for (int zoneIndex = 0; zoneIndex < mutableZones().size(); ++zoneIndex) {
            setZoneEffect(zoneIndex, lumacore::RgbEffect(
                lumacore::RgbEffectType::Static,
                lumacore::RgbColor(0, 0, 0),
                1.0,
                0
            ));
        }
        return true;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneColorWrite
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }

        return {
            lumacore::PermissionStatus::Denied,
            QStringLiteral("Group test device does not support %1.").arg(lumacore::backendCapabilityToString(capability)),
        };
    }
};

class ReadOnlyTestDevice final : public lumacore::RgbDevice
{
public:
    ReadOnlyTestDevice()
        : RgbDevice(
              QStringLiteral("read-only-device"),
              QStringLiteral("Read-only Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        setLikelyRgbController(true);
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Inventory Zone"),
            lumacore::RgbZoneType::AddressableHeader,
            1
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        Q_UNUSED(zoneIndex)
        Q_UNUSED(color)
        return false;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capability == lumacore::BackendCapability::DiscoveryRead) {
            return {lumacore::PermissionStatus::Granted, {}};
        }

        return {
            lumacore::PermissionStatus::Denied,
            QStringLiteral("Read-only test device does not expose %1.")
                .arg(lumacore::backendCapabilityToString(capability)),
        };
    }
};

class EffectOnlyConfirmationTestDevice final : public lumacore::RgbDevice
{
public:
    EffectOnlyConfirmationTestDevice()
        : RgbDevice(
              QStringLiteral("effect-only-confirmation-device"),
              QStringLiteral("Effect-only Confirmation Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Effect Zone"),
            lumacore::RgbZoneType::AddressableHeader,
            1
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        Q_UNUSED(zoneIndex)
        Q_UNUSED(color)
        return false;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capability == lumacore::BackendCapability::DiscoveryRead) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        if (capability == lumacore::BackendCapability::ZoneEffectWrite) {
            return {
                lumacore::PermissionStatus::RequiresConfirmation,
                QStringLiteral("Effect-only test device requires confirmation."),
            };
        }

        return {
            lumacore::PermissionStatus::Denied,
            QStringLiteral("Effect-only test device does not expose %1.")
                .arg(lumacore::backendCapabilityToString(capability)),
        };
    }
};

bool saveProfileFixture(
    const QString& profilesDirectory,
    const QString& profileName,
    const QJsonArray& devices
)
{
    const QString normalizedName = lumacore::ProfileStore::normalizeName(profileName);
    return lumacore::ProfileStore(profilesDirectory).save(
        normalizedName,
        QJsonObject {
            {QStringLiteral("formatVersion"), 1},
            {QStringLiteral("application"), QStringLiteral("LumaCore")},
            {QStringLiteral("profileName"), normalizedName},
            {QStringLiteral("devices"), devices},
        }
    );
}

QJsonObject profileZone(
    int index,
    const QString& name,
    const QString& color,
    const QString& effectType = QStringLiteral("Static")
)
{
    return QJsonObject {
        {QStringLiteral("index"), index},
        {QStringLiteral("name"), name},
        {QStringLiteral("type"), QStringLiteral("AddressableHeader")},
        {QStringLiteral("ledCount"), 1},
        {QStringLiteral("color"), color},
        {
            QStringLiteral("effect"),
            QJsonObject {
                {QStringLiteral("type"), effectType},
                {QStringLiteral("color"), color},
                {QStringLiteral("speed"), 1.0},
                {QStringLiteral("brightness"), 100},
            },
        },
    };
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    QTemporaryDir profileDirectory;
    if (!require(profileDirectory.isValid(), "temporary profile directory should be available")) {
        return 1;
    }
    QTemporaryDir settingsDirectory;
    if (!require(settingsDirectory.isValid(), "temporary settings directory should be available")) {
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("LumaCoreTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AppControllerTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    lumacore::DeviceManager manager(nullptr, profileDirectory.filePath(QStringLiteral("profiles")));
    manager.setDryRunEnabled(false);
    manager.registerBackend(std::make_unique<lumacore::MockBackend>());
    manager.initializeBackends(QStringLiteral("mock"));

    lumacore::AppController controller(&manager);
    if (!require(controller.backendId() == QStringLiteral("mock"), "controller should expose the active backend")
        || !require(controller.backendDeviceCount() == 1, "controller should expose the loaded device count")
        || !require(controller.zoneCount(0) > 0, "mock device should expose zones")
        || !require(controller.zoneCount(-1) == 0, "invalid devices should expose no zones")
        || !require(controller.zoneName(-1, -1).isEmpty(), "invalid zones should expose an empty name")
        || !require(controller.zoneLedCount(-1, -1) == 0, "invalid zones should expose zero LEDs")
        || !require(
            controller.zoneEffectType(-1, -1) == static_cast<int>(lumacore::RgbEffectType::Static),
            "invalid zones should preserve the static-effect fallback"
        )
        || !require(controller.deviceSupportsEffect(0, 0), "mock devices should support static effects")
        || !require(controller.deviceSupportsEffect(0, 1), "mock devices should support animated effects")
        || !require(!controller.deviceSupportsEffect(0, 99), "unknown effects should remain unsupported")
        || !require(!controller.deviceId(0).isEmpty(), "controller should expose stable device IDs")
        || !require(!controller.deviceName(0).isEmpty(), "controller should expose device display names")
        || !require(
            controller.deviceIndexForId(controller.deviceId(0)) == 0,
            "controller should resolve a device index from its stable ID"
        )
        || !require(
            controller.zoneIndexForName(0, controller.zoneName(0, 0)) == 0,
            "controller should resolve a zone index from its name"
        )
        || !require(
            controller.deviceIndexForId(QStringLiteral("missing-device")) == -1,
            "unknown device IDs should not resolve"
        )
        || !require(
            !controller.zoneEffectsPanelEnabled(0, 0),
            "selected-zone effects toggle should default off"
        )
        || !require(
            controller.zoneIndexForName(0, QStringLiteral("Missing Zone")) == -1,
            "unknown zone names should not resolve"
        )
        || !require(
            controller.daemonState() == QStringLiteral("Offline"),
            "controllers without a daemon client should report an offline state"
        )
        || !require(
            controller.setupStatusLevel() == QStringLiteral("ready"),
            "loaded writable local backends should report a ready setup state"
        )
        || !require(
            controller.setupStatusSummary() == QStringLiteral("Ready"),
            "ready setup state should expose a concise summary"
        )
        || !require(
            !controller.setupAttentionRequired(),
            "ready setup state should not require user attention"
        )
        || !require(
            !controller.rescanDaemonDevices(),
            "manual rescan should reject an offline daemon"
        )) {
        return 1;
    }

    controller.setZoneEffectsPanelEnabled(0, 0, true);
    {
        lumacore::AppController persistedController(&manager);
        if (!require(
                persistedController.zoneEffectsPanelEnabled(0, 0),
                "selected-zone effects toggle should persist per zone"
            )) {
            return 1;
        }
    }
    controller.setZoneEffectsPanelEnabled(0, 0, false);
    if (!require(
            !controller.zoneEffectsPanelEnabled(0, 0),
            "selected-zone effects toggle should save the off state"
        )) {
        return 1;
    }

    manager.setDryRunEnabled(true);
    if (!require(
            controller.setupStatusLevel() == QStringLiteral("warning"),
            "dry-run should be visible as a setup warning"
        )
        || !require(
            controller.setupStatusSummary() == QStringLiteral("Dry-run active"),
            "dry-run setup warning should explain why writes will not apply"
        )
        || !require(
            controller.setupAttentionRequired(),
            "dry-run setup warning should require user attention"
        )) {
        return 1;
    }
    manager.setDryRunEnabled(false);

    {
        lumacore::DeviceManager emptyManager(
            nullptr,
            profileDirectory.filePath(QStringLiteral("empty-profiles"))
        );
        lumacore::AppController emptyController(&emptyManager);
        if (!require(
                emptyController.setupStatusLevel() == QStringLiteral("warning"),
                "empty inventory should report a setup warning"
            )
            || !require(
                emptyController.setupStatusSummary() == QStringLiteral("No devices loaded"),
                "empty inventory warning should explain that no devices are available"
            )) {
            return 1;
        }
    }

    {
        lumacore::DeviceManager readOnlyManager(
            nullptr,
            profileDirectory.filePath(QStringLiteral("read-only-profiles"))
        );
        readOnlyManager.setDryRunEnabled(false);
        readOnlyManager.registerBackend(std::make_unique<lumacore::MockBackend>());
        if (!require(
                readOnlyManager.activateBackend(QStringLiteral("mock")),
                "read-only setup fixture should activate a write-capable backend descriptor"
            )) {
            return 1;
        }

        std::vector<std::unique_ptr<lumacore::RgbDevice>> readOnlyDevices;
        readOnlyDevices.push_back(std::make_unique<ReadOnlyTestDevice>());
        readOnlyManager.replaceDevices(std::move(readOnlyDevices));
        lumacore::AppController readOnlyController(&readOnlyManager);
        if (!require(
                readOnlyController.setupStatusSummary() == QStringLiteral("Read-only inventory"),
                "read-only devices should report inventory-only setup state"
            )
            || !require(
                readOnlyController.setupStatusDetail().contains(QStringLiteral("loaded devices")),
                "read-only setup detail should describe loaded device write verification"
            )
            || !require(
                readOnlyController.setupStatusDetail().contains(QStringLiteral("Read-only test device does not expose")),
                "read-only setup detail should include the first blocked write reason"
            )
            || !require(
                !readOnlyController.setupStatusDetail().contains(QStringLiteral("active backend")),
                "read-only setup detail should not blame the active backend descriptor"
            )
            || !require(
                readOnlyController.setupStatusAction().contains(QStringLiteral("write-gate verification")),
                "read-only setup action should direct users toward write-gate diagnostics"
            )) {
            return 1;
        }
    }

    {
        lumacore::DeviceManager effectOnlyManager(
            nullptr,
            profileDirectory.filePath(QStringLiteral("effect-only-profiles"))
        );
        effectOnlyManager.setDryRunEnabled(false);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> effectOnlyDevices;
        effectOnlyDevices.push_back(std::make_unique<EffectOnlyConfirmationTestDevice>());
        effectOnlyManager.replaceDevices(std::move(effectOnlyDevices));
        lumacore::AppController effectOnlyController(&effectOnlyManager);
        if (!require(
                effectOnlyController.deviceRequiresConfirmation(0),
                "effect-only write devices should still require confirmation in the controller"
            )) {
            return 1;
        }
    }

    {
        auto effectiveBackendClient = std::make_shared<lumacore::DaemonClient>(
            QStringLiteral("lumacore-app-controller-effective-backend-%1")
                .arg(QCoreApplication::applicationPid())
        );
        auto daemonBackend = std::make_unique<lumacore::DaemonBackend>(effectiveBackendClient);
        const std::vector<std::unique_ptr<lumacore::RgbDevice>> effectiveBackendDevices =
            daemonBackend->devicesFromPayload(QJsonObject {
            {QStringLiteral("backend"), lumacore::backendDescriptorToJson(lumacore::BackendDescriptor {
                QStringLiteral("auto"),
                QStringLiteral("Auto Backend"),
                QStringLiteral("Effective backend fixture"),
                lumacore::BackendCapability::DiscoveryRead | lumacore::BackendCapability::ZoneColorWrite,
            })},
            {QStringLiteral("devices"), QJsonArray {}},
        });
        Q_UNUSED(effectiveBackendDevices)

        lumacore::DeviceManager effectiveBackendManager(
            nullptr,
            profileDirectory.filePath(QStringLiteral("effective-backend-profiles"))
        );
        effectiveBackendManager.registerBackend(std::move(daemonBackend));
        if (!require(
                effectiveBackendManager.activateBackend(QStringLiteral("daemon")),
                "effective backend fixture should activate the daemon proxy backend"
            )) {
            return 1;
        }

        lumacore::AppController effectiveBackendController(
            &effectiveBackendManager,
            effectiveBackendClient
        );
        const QVariantMap effectiveDiagnostics = effectiveBackendController.diagnosticsReport();
        const QVariantMap effectiveDiagnosticBackend =
            effectiveDiagnostics.value(QStringLiteral("backend")).toMap();
        if (!require(
                effectiveBackendController.backendId() == QStringLiteral("daemon"),
                "controller backend ID should expose the GUI daemon proxy"
            )
            || !require(
                effectiveBackendController.backendEffectiveId() == QStringLiteral("auto"),
                "controller effective backend ID should expose the daemon-selected backend"
            )
            || !require(
                effectiveDiagnosticBackend.value(QStringLiteral("id")).toString() == QStringLiteral("daemon"),
                "diagnostics should preserve the proxy backend ID"
            )
            || !require(
                effectiveDiagnosticBackend.value(QStringLiteral("effectiveId")).toString() == QStringLiteral("auto"),
                "diagnostics should include the effective daemon backend ID"
            )) {
            return 1;
        }
    }

    {
        auto offlineClient = std::make_shared<lumacore::DaemonClient>(
            QStringLiteral("lumacore-app-controller-offline-%1")
                .arg(QCoreApplication::applicationPid())
        );
        lumacore::AppController offlineController(&manager, offlineClient);
        if (!require(
                offlineController.setupStatusSummary() == QStringLiteral("Daemon offline"),
                "offline daemon-backed controllers should expose daemon setup guidance"
            )
            || !require(
                offlineController.retryDaemonConnection(),
                "manual retry should queue an immediate daemon connection attempt"
            )
            || !require(
                offlineController.daemonRecoveryBusy(),
                "manual retry should expose a busy recovery state"
            )
            || !require(
                offlineController.daemonState() == QStringLiteral("Reconnecting"),
                "manual retry should expose the reconnecting state"
            )
            || !require(
                offlineController.setupStatusSummary() == QStringLiteral("Reconnecting to daemon"),
                "manual retry should update setup guidance to reconnecting"
            )) {
            return 1;
        }
        offlineClient->setAutomaticReconnectEnabled(false);
        offlineClient->disconnectFromDaemon();
    }

    {
        lumacore::DeviceManager selectionManager(
            nullptr,
            profileDirectory.filePath(QStringLiteral("selection-profiles"))
        );
        selectionManager.setDryRunEnabled(false);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> initialDevices;
        initialDevices.push_back(std::make_unique<StableIdDevice>(
            QStringLiteral("device-a"),
            QStringLiteral("Zone A")
        ));
        initialDevices.push_back(std::make_unique<StableIdDevice>(
            QStringLiteral("device-b"),
            QStringLiteral("Zone B")
        ));
        selectionManager.replaceDevices(std::move(initialDevices));
        lumacore::AppController selectionController(&selectionManager);

        const QString selectedId = selectionController.deviceId(1);
        const QString selectedZone = selectionController.zoneName(1, 0);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> reorderedDevices;
        reorderedDevices.push_back(std::make_unique<StableIdDevice>(
            QStringLiteral("device-b"),
            QStringLiteral("Zone B")
        ));
        reorderedDevices.push_back(std::make_unique<StableIdDevice>(
            QStringLiteral("device-a"),
            QStringLiteral("Zone A")
        ));
        selectionManager.replaceDevices(std::move(reorderedDevices));

        const int restoredDeviceIndex = selectionController.deviceIndexForId(selectedId);
        if (!require(restoredDeviceIndex == 0, "stable device IDs should survive reordered refreshes")
            || !require(
                selectionController.zoneIndexForName(restoredDeviceIndex, selectedZone) == 0,
                "zone names should restore selection on the reordered device"
            )) {
            return 1;
        }
    }

    {
        lumacore::DeviceManager groupManager(
            nullptr,
            profileDirectory.filePath(QStringLiteral("group-profiles"))
        );
        groupManager.setDryRunEnabled(false);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> groupDevices;
        groupDevices.push_back(std::make_unique<GroupTestDevice>(
            QStringLiteral("group-device-a"),
            QStringLiteral("Group Device A")
        ));
        groupDevices.push_back(std::make_unique<GroupTestDevice>(
            QStringLiteral("group-device-b"),
            QStringLiteral("Group Device B")
        ));
        groupManager.replaceDevices(std::move(groupDevices));
        lumacore::AppController groupController(&groupManager);

        QVariantMap groupResult;
        QObject::connect(
            &groupController,
            &lumacore::AppController::globalOperationFinished,
            &groupController,
            [&groupResult](const QVariantMap& result) { groupResult = result; }
        );

        const QColor groupColor(QStringLiteral("#204060"));
        if (!require(
                groupController.saveDeviceGroup(
                    QStringLiteral("Desk"),
                    QStringList {QStringLiteral("group-device-b"), QStringLiteral("missing-device")}
                ),
                "controller should save a named device group"
            )
            || !require(
                groupController.deviceGroupNames().contains(QStringLiteral("Desk")),
                "saved device groups should be listed"
            )
            || !require(
                groupController.deviceGroupDeviceIds(QStringLiteral("desk")).size() == 2,
                "device group lookup should be case-insensitive"
            )
            || !require(
                groupController.deviceGroupInfos().first().toMap().value(QStringLiteral("presentDeviceCount")).toInt() == 1,
                "device group info should count present devices separately from stale IDs"
            )
            || !require(
                groupController.applyEffectToDeviceGroup(
                    QStringLiteral("Desk"),
                    static_cast<int>(lumacore::RgbEffectType::Static),
                    groupColor,
                    1.0,
                    70
                ),
                "group effects should start for a non-empty group"
            )
            || !require(
                groupResult.value(QStringLiteral("targetKind")).toString() == QStringLiteral("group"),
                "group operation results should identify the target kind"
            )
            || !require(
                groupResult.value(QStringLiteral("targetName")).toString() == QStringLiteral("Desk"),
                "group operation results should identify the target name"
            )
            || !require(
                groupResult.value(QStringLiteral("applied")).toInt() == 1,
                "group effects should apply only to present grouped devices"
            )
            || !require(
                groupController.zoneEffectColor(1, 0) == groupColor,
                "group effects should update grouped devices"
            )
            || !require(
                groupController.zoneEffectColor(0, 0) != groupColor,
                "group effects should not update devices outside the group"
            )
            || !require(
                groupController.setDeviceGroupBrightness(QStringLiteral("Desk"), 35),
                "group brightness should start for a non-empty group"
            )
            || !require(
                groupController.zoneEffectBrightness(1, 0) == 35,
                "group brightness should update grouped devices"
            )
            || !require(
                groupController.allOffDeviceGroup(QStringLiteral("Desk")),
                "group All Off should start for a non-empty group"
            )
            || !require(
                groupController.zoneEffectBrightness(1, 0) == 0,
                "group All Off should turn off grouped devices"
            )
            || !require(
                groupController.deleteDeviceGroup(QStringLiteral("Desk")),
                "controller should delete a device group"
            )
            || !require(
                groupController.deviceGroupNames().isEmpty(),
                "deleted device groups should be removed from the list"
            )) {
            return 1;
        }
    }

    QVariantMap globalResult;
    QObject::connect(
        &controller,
        &lumacore::AppController::globalOperationFinished,
        &controller,
        [&globalResult](const QVariantMap& result) { globalResult = result; }
    );

    const QColor globalColor(QStringLiteral("#336699"));
    if (!require(
            controller.applyEffectGlobally(2, globalColor, 1.5, 60),
            "global effects should start when writable zones are available"
        )
        || !require(
            globalResult.value(QStringLiteral("applied")).toInt() == controller.zoneCount(0),
            "global effect result should report every applied zone"
        )) {
        return 1;
    }
    for (int zoneIndex = 0; zoneIndex < controller.zoneCount(0); ++zoneIndex) {
        if (!require(
                controller.zoneEffectType(0, zoneIndex) == static_cast<int>(lumacore::RgbEffectType::Breathing),
                "global effects should update every compatible zone"
            )
            || !require(
                controller.zoneEffectBrightness(0, zoneIndex) == 60,
                "global effects should apply the selected brightness"
            )) {
            return 1;
        }
    }
    if (!require(controller.setZoneBrightness(0, 0, 35), "selected-zone brightness should start")
        || !require(
            controller.zoneEffectType(0, 0) == static_cast<int>(lumacore::RgbEffectType::Breathing),
            "selected-zone brightness should preserve the current effect"
        )
        || !require(
            controller.zoneEffectBrightness(0, 0) == 35,
            "selected-zone brightness should update the selected zone brightness"
        )) {
        return 1;
    }
    if (!require(controller.setGlobalBrightness(25), "global brightness should start")
        || !require(
            globalResult.value(QStringLiteral("operation")).toString() == QStringLiteral("Global brightness"),
            "global brightness should publish a structured result"
        )
        || !require(
            controller.zoneEffectType(0, 0) == static_cast<int>(lumacore::RgbEffectType::Breathing),
            "global brightness should preserve the current effect"
        )
        || !require(
            controller.zoneEffectBrightness(0, 0) == 25,
            "global brightness should update the current effect brightness"
        )
        || !require(controller.allOffAllDevices(), "global All Off should start")
        || !require(
            globalResult.value(QStringLiteral("applied")).toInt() == 1,
            "global All Off should report applied devices"
        )
        || !require(
            controller.zoneEffectBrightness(0, 0) == 0,
            "global All Off should turn zones off"
        )) {
        return 1;
    }

    const QVariantMap diagnostics = controller.diagnosticsReport();
    const QVariantList diagnosticDevices = diagnostics.value(QStringLiteral("devices")).toList();
    const QVariantList diagnosticZones = diagnosticDevices.isEmpty()
        ? QVariantList {}
        : diagnosticDevices.first().toMap().value(QStringLiteral("zones")).toList();
    const QVariantMap diagnosticSetup = diagnostics.value(QStringLiteral("setup")).toMap();
    const QVariantMap diagnosticSummary = diagnostics.value(QStringLiteral("summary")).toMap();
    const QVariantMap diagnosticScope = diagnostics.value(QStringLiteral("diagnosticScope")).toMap();
    const QVariantMap diagnosticCounts = diagnostics.value(QStringLiteral("counts")).toMap();
    const QVariantMap diagnosticApplication = diagnostics.value(QStringLiteral("application")).toMap();
    const QVariantMap diagnosticBackend = diagnostics.value(QStringLiteral("backend")).toMap();
    const QVariantMap diagnosticPackage = diagnostics.value(QStringLiteral("package")).toMap();
    const QVariantMap diagnosticStorage = diagnostics.value(QStringLiteral("storage")).toMap();
    const QString diagnosticSummaryText = controller.diagnosticsSummaryText();
    if (!require(
            diagnostics.value(QStringLiteral("schemaVersion")).toInt() == 1,
            "diagnostics should expose a stable schema version"
        )
        || !require(
            diagnosticSummary.value(QStringLiteral("setup")).toString() == QStringLiteral("Ready"),
            "diagnostics should include a top-level support summary"
        )
        || !require(
            !diagnosticScope.value(QStringLiteral("profileContentsIncluded")).toBool(),
            "diagnostics should explicitly state that profile contents are excluded"
        )
        || !require(
            diagnosticScope.value(QStringLiteral("profileNamesIncluded")).toBool(),
            "diagnostics should explicitly state that profile names are included"
        )
        || !require(
            diagnostics.value(QStringLiteral("backend")).toMap().value(QStringLiteral("id")).toString()
                == QStringLiteral("mock"),
            "diagnostics should include the active backend"
        )
        || !require(
            diagnosticBackend.value(QStringLiteral("availableBackendIds")).toStringList().contains(QStringLiteral("mock")),
            "diagnostics should include registered backend ids"
        )
        || !require(
            !diagnosticApplication.value(QStringLiteral("executablePath")).toString().isEmpty(),
            "diagnostics should include the GUI executable path"
        )
        || !require(
            diagnosticPackage.contains(QStringLiteral("bundledDaemonPresent")),
            "diagnostics should include bundled daemon package health"
        )
        || !require(
            diagnosticStorage.value(QStringLiteral("profilesDirectory")).toString() == QStringLiteral("<profiles>"),
            "diagnostics should include the redacted active profiles directory"
        )
        || !require(!diagnosticDevices.isEmpty(), "diagnostics should include device summaries")
        || !require(
            diagnosticZones.size() == controller.zoneCount(0),
            "diagnostics should include zone summaries"
        )
        || !require(
            diagnosticCounts.value(QStringLiteral("devices")).toInt() == controller.backendDeviceCount(),
            "diagnostics should include device counts"
        )
        || !require(
            diagnosticCounts.value(QStringLiteral("zones")).toInt() == controller.zoneCount(0),
            "diagnostics should include zone counts"
        )
        || !require(
            diagnosticCounts.value(QStringLiteral("writableDevices")).toInt() > 0,
            "diagnostics should include writable device counts"
        )
        || !require(
            diagnosticSetup.value(QStringLiteral("summary")).toString() == QStringLiteral("Ready"),
            "diagnostics should include the derived setup summary"
        )
        || !require(
            !diagnosticSetup.value(QStringLiteral("attentionRequired")).toBool(),
            "diagnostics should include setup attention state"
        )
        || !require(
            diagnosticDevices.first().toMap().value(QStringLiteral("supportedEffects")).toList().size() == 4,
            "diagnostics should include device-level effect support"
        )
        || !require(
            diagnosticZones.first().toMap().value(QStringLiteral("supportedEffects")).toList().size() == 4,
            "diagnostics should include zone-level effect support"
        )
        || !require(
            diagnostics.value(QStringLiteral("activity")).toList().size() > 0,
            "diagnostics should include recent activity"
        )
        || !require(
            diagnosticSummaryText.contains(QStringLiteral("LumaCore diagnostics summary")),
            "diagnostics summary text should include a clear heading"
        )
        || !require(
            diagnosticSummaryText.contains(QStringLiteral("profile contents are not included")),
            "diagnostics summary text should state that profile contents are excluded"
        )) {
        return 1;
    }

    const QString diagnosticsPath = profileDirectory.filePath(QStringLiteral("diagnostics.json"));
    if (!require(
            controller.exportDiagnostics(QUrl::fromLocalFile(diagnosticsPath)),
            "controller should export diagnostics"
        )
        || !require(QFile::exists(diagnosticsPath), "diagnostics export should create a file")) {
        return 1;
    }

    QFile diagnosticsFile(diagnosticsPath);
    if (!require(diagnosticsFile.open(QIODevice::ReadOnly), "diagnostics export should be readable")) {
        return 1;
    }
    const QJsonDocument diagnosticsDocument = QJsonDocument::fromJson(diagnosticsFile.readAll());
    if (!require(diagnosticsDocument.isObject(), "diagnostics export should contain JSON object data")
        || !require(
            diagnosticsDocument.object()
                .value(QStringLiteral("devices"))
                .toArray()
                .size() == controller.backendDeviceCount(),
            "diagnostics export should preserve the device count"
        )) {
        return 1;
    }

    const QColor savedColor(QStringLiteral("#112233"));
    const QColor changedColor(QStringLiteral("#AABBCC"));
    if (!require(
            controller.applyEffect(0, 0, 0, savedColor, 1.0, 100),
            "saved startup color should apply"
        )
        || !require(controller.saveProfile(QStringLiteral("Evening")), "startup profile should save")
        || !require(
            controller.applyEffect(0, 0, 0, changedColor, 1.0, 100),
            "changed startup color should apply"
        )
        || !require(controller.applyProfileOnLaunch(QStringLiteral("Evening")), "active profile should apply on launch")
        || !require(
            controller.zoneEffectColor(0, 0) == savedColor,
            "startup profile should restore the saved zone color"
        )
        || !require(
            controller.statusMessage() == QStringLiteral("Applied active profile 'Evening' on launch."),
            "successful startup application should be visible in status"
        )
        || !require(
            !controller.applyProfileOnLaunch(QString()),
            "startup application should reject an empty active profile"
        )) {
        return 1;
    }

    // Without a daemon client, arming the launch apply applies immediately
    // through the same path as applyProfileOnLaunch.
    if (!require(
            controller.applyEffect(0, 0, static_cast<int>(lumacore::RgbEffectType::Static), changedColor, 1.0, 100),
            "pre-arm color should apply"
        )) {
        return 1;
    }
    controller.armLaunchProfileApply(QStringLiteral("Evening"));
    if (!require(
            controller.zoneEffectColor(0, 0) == savedColor,
            "arming with local devices should apply the launch profile immediately"
        )
        || !require(
            controller.statusMessage() == QStringLiteral("Applied active profile 'Evening' on launch."),
            "immediate armed applies should set the launch status message"
        )) {
        return 1;
    }

    const QString exportPath = profileDirectory.filePath(QStringLiteral("Evening-export.json"));
    if (!require(
            controller.exportProfile(QStringLiteral("Evening"), QUrl::fromLocalFile(exportPath)),
            "controller should export a profile"
        )
        || !require(QFile::exists(exportPath), "controller export should create the selected file")
        || !require(controller.deleteProfile(QStringLiteral("Evening")), "exported source profile should delete")
        || !require(
            controller.importProfile(QUrl::fromLocalFile(exportPath)) == QStringLiteral("Evening"),
            "controller should import and return the profile name"
        )
        || !require(controller.profileExists(QStringLiteral("Evening")), "saved profile should be reported as existing")
        || !require(
            controller.profileExists(QStringLiteral(" Evening ")),
            "profile existence checks should normalize names"
        )
        || !require(!controller.profileExists(QStringLiteral("Missing")), "missing profile should not exist")) {
        return 1;
    }

    const QVariantMap eveningPreview = controller.profileCompatibility(QStringLiteral("Evening"));
    const QVariantList eveningPreviewItems = eveningPreview.value(QStringLiteral("previewItems")).toList();
    if (!require(
            eveningPreview.value(QStringLiteral("applicableZones")).toInt() > 0,
            "controller should expose compatibility before applying a profile"
        )
        || !require(!eveningPreviewItems.isEmpty(), "controller should expose profile apply preview rows")
        || !require(
            eveningPreviewItems.first().toMap().contains(QStringLiteral("targetEffect")),
            "profile apply preview rows should expose target effects"
        )) {
        return 1;
    }

    if (!require(
            !controller.applyProfileWithReport(QStringLiteral("Evening"))
                 .value(QStringLiteral("partial"))
                 .toBool(),
            "fully compatible profiles should not report a partial result"
        )) {
        return 1;
    }

    QVariantMap asyncProfileResult;
    QObject::connect(
        &controller,
        &lumacore::AppController::profileApplyFinished,
        &controller,
        [&asyncProfileResult](const QVariantMap& result) { asyncProfileResult = result; }
    );
    if (!require(
            controller.applyEffect(0, 0, 0, changedColor, 1.0, 100),
            "changed async profile color should apply"
        )
        || !require(
            controller.applyProfileAsync(QStringLiteral("Evening")),
            "async profile apply should start for compatible mock profiles"
        )
        || !require(
            !controller.profileApplyInProgress(),
            "local async profile fallback should finish immediately"
        )
        || !require(
            asyncProfileResult.value(QStringLiteral("success")).toBool(),
            "async profile apply should report success"
        )
        || !require(
            !asyncProfileResult.value(QStringLiteral("partial")).toBool(),
            "fully compatible async profile apply should not report partial"
        )
        || !require(
            controller.zoneEffectColor(0, 0) == savedColor,
            "async profile apply should restore the saved zone color"
        )) {
        return 1;
    }

    {
        const QString profilePath = profileDirectory.filePath(QStringLiteral("async-partial-profiles"));
        lumacore::DeviceManager asyncManager(nullptr, profilePath);
        asyncManager.setDryRunEnabled(false);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> devices;
        devices.push_back(std::make_unique<StableIdDevice>(
            QStringLiteral("profile-device"),
            QStringLiteral("Zone A")
        ));
        asyncManager.replaceDevices(std::move(devices));
        lumacore::AppController asyncController(&asyncManager);

        QVariantMap partialResult;
        QObject::connect(
            &asyncController,
            &lumacore::AppController::profileApplyFinished,
            &asyncController,
            [&partialResult](const QVariantMap& result) { partialResult = result; }
        );

        const QJsonArray partialDevices {
            QJsonObject {
                {QStringLiteral("id"), QStringLiteral("profile-device")},
                {QStringLiteral("name"), QStringLiteral("Profile Device")},
                {
                    QStringLiteral("zones"),
                    QJsonArray {
                        profileZone(0, QStringLiteral("Zone A"), QStringLiteral("#102030")),
                        profileZone(9, QStringLiteral("Missing Zone"), QStringLiteral("#203040")),
                        profileZone(0, QStringLiteral("Zone A"), QStringLiteral("not-a-color")),
                        profileZone(0, QStringLiteral("Zone A"), QStringLiteral("#405060"), QStringLiteral("Rainbow")),
                    },
                },
            },
            QJsonObject {
                {QStringLiteral("id"), QStringLiteral("missing-device")},
                {QStringLiteral("name"), QStringLiteral("Missing Device")},
                {
                    QStringLiteral("zones"),
                    QJsonArray {
                        profileZone(0, QStringLiteral("Zone A"), QStringLiteral("#506070")),
                    },
                },
            },
        };

        if (!require(
                saveProfileFixture(profilePath, QStringLiteral("partial"), partialDevices),
                "partial async profile fixture should save"
            )
            || !require(
                asyncController.applyProfileAsync(QStringLiteral("partial")),
                "partial async profile should start"
            )
            || !require(
                partialResult.value(QStringLiteral("success")).toBool(),
                "partial async profile should report success when one zone applies"
            )
            || !require(
                partialResult.value(QStringLiteral("partial")).toBool(),
                "partial async profile should report partial"
            )
            || !require(
                partialResult.value(QStringLiteral("appliedZones")).toInt() == 1,
                "partial async profile should count applied zones"
            )
            || !require(
                partialResult.value(QStringLiteral("missingDeviceZones")).toInt() == 1,
                "partial async profile should count missing-device zones"
            )
            || !require(
                partialResult.value(QStringLiteral("missingZones")).toInt() == 1,
                "partial async profile should count missing zones"
            )
            || !require(
                partialResult.value(QStringLiteral("invalidZones")).toInt() == 1,
                "partial async profile should count invalid zones"
            )
            || !require(
                partialResult.value(QStringLiteral("unsupportedZones")).toInt() == 1,
                "partial async profile should count unsupported zones"
            )
            || !require(
                asyncController.zoneEffectColor(0, 0) == QColor(QStringLiteral("#102030")),
                "partial async profile should apply the compatible zone"
            )) {
            return 1;
        }

        QVariantMap noMatchResult;
        QObject::connect(
            &asyncController,
            &lumacore::AppController::profileApplyFinished,
            &asyncController,
            [&noMatchResult](const QVariantMap& result) { noMatchResult = result; }
        );
        const QJsonArray noMatchDevices {
            QJsonObject {
                {QStringLiteral("id"), QStringLiteral("missing-device")},
                {QStringLiteral("name"), QStringLiteral("Missing Device")},
                {
                    QStringLiteral("zones"),
                    QJsonArray {
                        profileZone(0, QStringLiteral("Zone A"), QStringLiteral("#506070")),
                    },
                },
            },
        };
        if (!require(
                saveProfileFixture(profilePath, QStringLiteral("no-match"), noMatchDevices),
                "no-match async profile fixture should save"
            )
            || !require(
                asyncController.applyProfileAsync(QStringLiteral("no-match")),
                "no-match async profile should start and finish with a report"
            )
            || !require(
                !noMatchResult.value(QStringLiteral("success")).toBool(),
                "no-match async profile should report failure"
            )
            || !require(
                noMatchResult.value(QStringLiteral("missingDeviceZones")).toInt() == 1,
                "no-match async profile should count missing-device zones"
            )) {
            return 1;
        }
    }

    {
        const QString daemonProfilePath = profileDirectory.filePath(QStringLiteral("async-daemon-profiles"));
        lumacore::DeviceManager daemonManager(nullptr, daemonProfilePath);
        daemonManager.setDryRunEnabled(false);
        auto daemonClient = std::make_shared<lumacore::DaemonClient>(
            QStringLiteral("lumacore-app-controller-async-profile-%1").arg(QCoreApplication::applicationPid())
        );
        QJsonObject daemonSnapshot {
            {QStringLiteral("index"), 0},
            {QStringLiteral("id"), QStringLiteral("daemon-device")},
            {QStringLiteral("name"), QStringLiteral("Daemon Device")},
            {QStringLiteral("vendor"), QStringLiteral("LumaCore")},
            {QStringLiteral("type"), QStringLiteral("Controller")},
            {
                QStringLiteral("capabilities"),
                QJsonArray {
                    QStringLiteral("DiscoveryRead"),
                    QStringLiteral("ZoneColorWrite"),
                    QStringLiteral("ZoneEffectWrite"),
                },
            },
            {
                QStringLiteral("zones"),
                QJsonArray {
                    QJsonObject {
                        {QStringLiteral("name"), QStringLiteral("Zone A")},
                        {QStringLiteral("type"), QStringLiteral("AddressableHeader")},
                        {QStringLiteral("ledCount"), 1},
                        {QStringLiteral("color"), QStringLiteral("#000000")},
                        {
                            QStringLiteral("effectSupport"),
                            QJsonArray {
                                QJsonObject {
                                    {QStringLiteral("effectType"), 0},
                                    {QStringLiteral("supported"), true},
                                    {QStringLiteral("speed"), false},
                                    {QStringLiteral("brightness"), true},
                                },
                            },
                        },
                    },
                },
            },
        };
        std::vector<std::unique_ptr<lumacore::RgbDevice>> daemonDevices;
        daemonDevices.push_back(std::make_unique<lumacore::DaemonRgbDevice>(
            daemonSnapshot,
            daemonClient
        ));
        daemonManager.replaceDevices(std::move(daemonDevices));
        lumacore::AppController daemonController(&daemonManager, daemonClient);
        if (!require(
                saveProfileFixture(
                    daemonProfilePath,
                    QStringLiteral("daemon-pending"),
                    QJsonArray {
                        QJsonObject {
                            {QStringLiteral("id"), QStringLiteral("daemon-device")},
                            {QStringLiteral("name"), QStringLiteral("Daemon Device")},
                            {
                                QStringLiteral("zones"),
                                QJsonArray {
                                    profileZone(0, QStringLiteral("Zone A"), QStringLiteral("#123456")),
                                },
                            },
                        },
                    }
                ),
                "daemon async profile fixture should save"
            )) {
            return 1;
        }

        QElapsedTimer elapsed;
        elapsed.start();
        if (!require(
                daemonController.applyProfileAsync(QStringLiteral("daemon-pending")),
                "daemon async profile should start without a connected daemon"
            )
            || !require(
                elapsed.elapsed() < 500,
                "daemon async profile should not block on the synchronous daemon call path"
            )
            || !require(
                daemonController.profileApplyInProgress(),
                "daemon async profile should remain in progress while daemon requests are pending"
            )
            || !require(
                !daemonController.applyProfileAsync(QStringLiteral("daemon-pending")),
                "duplicate async profile applies should be rejected while one is pending"
            )) {
            return 1;
        }
        daemonClient->disconnectFromDaemon();
    }

    // A daemon-backed controller with no device snapshot yet parks a scheduled
    // apply instead of burning the day's attempt against an empty device set.
    {
        QTemporaryDir pendingProfilesDirectory;
        if (!require(pendingProfilesDirectory.isValid(), "pending schedule profile directory should be available")) {
            return 1;
        }
        auto pendingClient = std::make_shared<lumacore::DaemonClient>(
            QStringLiteral("lumacore-app-controller-pending-%1").arg(QCoreApplication::applicationPid())
        );
        lumacore::DeviceManager pendingManager(
            nullptr,
            pendingProfilesDirectory.filePath(QStringLiteral("profiles"))
        );
        lumacore::AppController pendingController(&pendingManager, pendingClient);
        if (!require(
                pendingController.applyScheduledProfile(QStringLiteral("Evening")),
                "scheduled applies should park while no daemon snapshot has arrived"
            )
            || !require(
                pendingController.statusMessage().contains(QStringLiteral("Waiting for daemon devices")),
                "parked scheduled applies should report that they are waiting for devices"
            )
            || !require(
                !pendingController.profileApplyInProgress(),
                "parked scheduled applies should not start a profile apply"
            )) {
            return 1;
        }

        // Arming the launch apply parks it the same way while no snapshot has
        // arrived, without starting an apply or reporting an error.
        pendingController.armLaunchProfileApply(QStringLiteral("Evening"));
        if (!require(
                !pendingController.profileApplyInProgress(),
                "armed launch applies should wait for the first daemon snapshot"
            )) {
            return 1;
        }
        pendingController.armLaunchProfileApply(QStringLiteral("   "));
        if (!require(
                pendingController.statusMessage() == QStringLiteral("No active profile is selected for launch."),
                "arming with an empty profile should keep the launch status message"
            )) {
            return 1;
        }
    }

    // Dry-run applies must keep animated effects visible: the effects engine
    // streams frames into the local zone model without daemon writes, so the
    // zone preview color keeps changing while dry-run is enabled.
    {
        lumacore::DeviceManager animServerManager;
        animServerManager.setDryRunEnabled(true);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> animServerDevices;
        animServerDevices.push_back(std::make_unique<AnimPreviewDevice>());
        animServerManager.replaceDevices(std::move(animServerDevices));
        lumacore::DaemonServer animServer(&animServerManager);
        const QString animServerName =
            QStringLiteral("lumacore-app-controller-anim-%1").arg(QCoreApplication::applicationPid());
        QString animListenError;
        if (!require(animServer.listen(animServerName, &animListenError), "anim preview server should listen")) {
            return 1;
        }

        QTemporaryDir animProfilesDirectory;
        auto animClient = std::make_shared<lumacore::DaemonClient>(animServerName);
        lumacore::DeviceManager animManager(nullptr, animProfilesDirectory.filePath(QStringLiteral("profiles")));
        animManager.setDryRunEnabled(true);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> animProxies;
        animProxies.push_back(std::make_unique<lumacore::DaemonRgbDevice>(
            lumacore::deviceToJson(*animServerManager.deviceAt(0), 0, false),
            animClient
        ));
        animManager.replaceDevices(std::move(animProxies));
        lumacore::AppController animController(&animManager, animClient);

        const auto waitUntilAnim = [](const std::function<bool()>& condition, int timeoutMs) {
            QElapsedTimer waitTimer;
            waitTimer.start();
            while (!condition() && waitTimer.elapsed() < timeoutMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            return condition();
        };

        if (!require(
                animController.applyEffect(
                    0,
                    0,
                    static_cast<int>(lumacore::RgbEffectType::Rainbow),
                    QColor(QStringLiteral("#ff0000")),
                    1.0,
                    100
                ),
                "dry-run rainbow apply should start"
            )
            || !require(
                waitUntilAnim(
                    [&animController] {
                        return animController.zoneEffectType(0, 0)
                            == static_cast<int>(lumacore::RgbEffectType::Rainbow);
                    },
                    3000
                ),
                "dry-run rainbow apply should complete against the in-process daemon"
            )) {
            return 1;
        }

        const lumacore::RgbColor firstSample = animManager.deviceAt(0)->zones().at(0).currentColor();
        const bool animated = waitUntilAnim(
            [&animManager, firstSample] {
                return !(animManager.deviceAt(0)->zones().at(0).currentColor() == firstSample);
            },
            3000
        );
        if (!require(animated, "dry-run animated effects should keep updating the local zone preview")) {
            return 1;
        }
        animClient->disconnectFromDaemon();
    }

    // The setup-status banner must track per-session hardware-write confirmation
    // live: confirming a write-gated device clears "Hardware confirmation
    // required" and revoking brings it back, both without waiting for a device
    // rescan. Regression for a banner that only refreshed on rescan.
    {
        lumacore::DeviceManager confirmServerManager;
        confirmServerManager.setDryRunEnabled(false);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> confirmServerDevices;
        confirmServerDevices.push_back(std::make_unique<EffectOnlyConfirmationTestDevice>());
        confirmServerManager.replaceDevices(std::move(confirmServerDevices));
        lumacore::DaemonServer confirmServer(&confirmServerManager);
        const QString confirmServerName =
            QStringLiteral("lumacore-app-controller-confirm-%1").arg(QCoreApplication::applicationPid());
        QString confirmListenError;
        if (!require(confirmServer.listen(confirmServerName, &confirmListenError), "confirm banner server should listen")) {
            return 1;
        }

        QTemporaryDir confirmProfilesDirectory;
        auto confirmClient = std::make_shared<lumacore::DaemonClient>(confirmServerName);
        if (!require(confirmClient->connectToDaemon(3000), "confirm banner client should connect")) {
            return 1;
        }
        lumacore::DeviceManager confirmManager(nullptr, confirmProfilesDirectory.filePath(QStringLiteral("profiles")));
        confirmManager.setDryRunEnabled(false);
        confirmManager.registerBackend(std::make_unique<lumacore::DaemonBackend>(confirmClient));
        if (!require(
                confirmManager.activateBackend(QStringLiteral("daemon")),
                "confirm banner fixture should activate the daemon proxy backend"
            )) {
            return 1;
        }
        lumacore::AppController confirmController(&confirmManager, confirmClient);

        const auto pumpUntil = [](const std::function<bool()>& condition, int timeoutMs) {
            QElapsedTimer timer;
            timer.start();
            while (!condition() && timer.elapsed() < timeoutMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            return condition();
        };
        const auto rescanAndWait = [&confirmController, &pumpUntil] {
            if (!confirmController.rescanDaemonDevices()) {
                return false;
            }
            return pumpUntil(
                [&confirmController] {
                    return confirmController.daemonState() == QStringLiteral("Connected");
                },
                3000
            );
        };

        if (!require(rescanAndWait(), "initial daemon device load should complete")
            || !require(
                confirmManager.deviceCount() == 1,
                "confirm banner fixture should load the daemon device proxy"
            )
            || !require(
                confirmController.setupStatusSummary() == QStringLiteral("Hardware confirmation required"),
                "setup banner should report confirmation required before confirming"
            )) {
            return 1;
        }

        confirmController.confirmDeviceWrites(0);
        const bool confirmedLive = pumpUntil(
            [&confirmController] { return confirmController.deviceWriteConfirmed(0); },
            3000
        );
        if (!require(confirmedLive, "device should report confirmed after the confirm response lands")
            || !require(
                confirmController.setupStatusSummary() == QStringLiteral("Ready"),
                "setup banner should clear to Ready as soon as writes are confirmed, without a rescan"
            )) {
            return 1;
        }

        // Reload proxies from a snapshot taken while confirmed, then revoke.
        // The banner must come back: the confirmation requirement has to stay
        // recoverable across a rescan in the confirmed state.
        if (!require(rescanAndWait(), "rescan while confirmed should complete")
            || !require(
                confirmController.deviceWriteConfirmed(0),
                "rescan while confirmed should preserve the confirmed state"
            )
            || !require(
                confirmController.setupStatusSummary() == QStringLiteral("Ready"),
                "setup banner should stay Ready across a rescan while confirmed"
            )) {
            return 1;
        }

        confirmController.revokeDeviceWrites(0);
        const bool revokedLive = pumpUntil(
            [&confirmController] { return !confirmController.deviceWriteConfirmed(0); },
            3000
        );
        if (!require(revokedLive, "device should report unconfirmed after the revoke response lands")
            || !require(
                confirmController.deviceRequiresConfirmation(0),
                "revoking after a confirmed-state rescan should restore the confirmation requirement"
            )
            || !require(
                confirmController.setupStatusSummary() == QStringLiteral("Hardware confirmation required"),
                "setup banner should return to confirmation required as soon as writes are revoked, without a rescan"
            )) {
            return 1;
        }

        // A rescan in the revoked state must agree with the live update.
        if (!require(rescanAndWait(), "rescan after revoking should complete")
            || !require(
                confirmController.setupStatusSummary() == QStringLiteral("Hardware confirmation required"),
                "setup banner should keep requiring confirmation after a rescan in the revoked state"
            )) {
            return 1;
        }

        confirmClient->disconnectFromDaemon();
        confirmServer.close();
    }

    return 0;
}
