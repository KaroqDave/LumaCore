// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockBackend.h"
#include "backends/daemon/DaemonRgbDevice.h"
#include "core/DeviceManager.h"
#include "core/ProfileStore.h"
#include "ui/AppController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

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

    return 0;
}
