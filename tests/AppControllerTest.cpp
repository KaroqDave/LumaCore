#include "backends/mock/MockBackend.h"
#include "core/DeviceManager.h"
#include "ui/AppController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUrl>

#include <utility>
#include <vector>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
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
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    QTemporaryDir profileDirectory;
    if (!require(profileDirectory.isValid(), "temporary profile directory should be available")) {
        return 1;
    }

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
            controller.zoneIndexForName(0, QStringLiteral("Missing Zone")) == -1,
            "unknown zone names should not resolve"
        )
        || !require(
            controller.daemonState() == QStringLiteral("Offline"),
            "controllers without a daemon client should report an offline state"
        )
        || !require(
            !controller.rescanDaemonDevices(),
            "manual rescan should reject an offline daemon"
        )) {
        return 1;
    }

    {
        auto offlineClient = std::make_shared<lumacore::DaemonClient>(
            QStringLiteral("lumacore-app-controller-offline-%1")
                .arg(QCoreApplication::applicationPid())
        );
        lumacore::AppController offlineController(&manager, offlineClient);
        if (!require(
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
    if (!require(
            diagnostics.value(QStringLiteral("schemaVersion")).toInt() == 1,
            "diagnostics should expose a stable schema version"
        )
        || !require(
            diagnostics.value(QStringLiteral("backend")).toMap().value(QStringLiteral("id")).toString()
                == QStringLiteral("mock"),
            "diagnostics should include the active backend"
        )
        || !require(!diagnosticDevices.isEmpty(), "diagnostics should include device summaries")
        || !require(
            diagnosticDevices.first().toMap().value(QStringLiteral("zones")).toList().size()
                == controller.zoneCount(0),
            "diagnostics should include zone summaries"
        )
        || !require(
            diagnostics.value(QStringLiteral("activity")).toList().size() > 0,
            "diagnostics should include recent activity"
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
        || !require(
            controller.profileCompatibility(QStringLiteral("Evening"))
                .value(QStringLiteral("applicableZones"))
                .toInt() > 0,
            "controller should expose compatibility before applying a profile"
        )
        || !require(controller.profileExists(QStringLiteral("Evening")), "saved profile should be reported as existing")
        || !require(
            controller.profileExists(QStringLiteral(" Evening ")),
            "profile existence checks should normalize names"
        )
        || !require(!controller.profileExists(QStringLiteral("Missing")), "missing profile should not exist")
        || !require(
            !controller.applyProfileWithReport(QStringLiteral("Evening"))
                 .value(QStringLiteral("partial"))
                 .toBool(),
            "fully compatible profiles should not report a partial result"
        )) {
        return 1;
    }

    return 0;
}
