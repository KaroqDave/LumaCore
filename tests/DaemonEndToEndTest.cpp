// SPDX-License-Identifier: GPL-2.0-or-later

// Full-stack end-to-end coverage for the real daemon binary. Unlike
// DaemonProtocolTest, which drives an in-process DeviceManager over a bare
// QLocalServer, this test launches lumacore-daemon with the mock backend and
// exercises the exact flow a GUI client performs: handshake, device inventory,
// dry-run effect and frame writes through the streaming path, All Off, activity
// log snapshot, the dry-run synchronization guard, and idle shutdown.

#include "app/Version.h"
#include "core/RgbColor.h"
#include "core/RgbEffect.h"
#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QThread>
#include <QVector>

#include <memory>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

QString uniqueEndpoint(const QString& suffix)
{
    return QStringLiteral("lumacore-e2e-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(suffix);
}

bool waitForConnection(const std::shared_ptr<lumacore::DaemonClient>& client, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (client->connectToDaemon(100)) {
            return true;
        }
        QThread::msleep(25);
    }
    return false;
}

bool waitForCompleteInventory(
    const std::shared_ptr<lumacore::DaemonClient>& client,
    lumacore::DaemonCallResult* result,
    int timeoutMs = 5000
)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const lumacore::DaemonCallResult response =
            client->call(lumacore::daemonMethodName(lumacore::DaemonMethod::ListDevices), {}, 3000);
        if (response.ok
            && response.result.value(QStringLiteral("discoveryComplete")).toBool(true)) {
            if (result != nullptr) {
                *result = response;
            }
            return true;
        }
        QThread::msleep(25);
    }
    return false;
}

void startMockDaemon(
    QProcess& process,
    const QString& daemonPath,
    const QString& endpoint,
    const QString& scenarioId = {}
)
{
    QStringList arguments {
        QStringLiteral("--backend"),
        QStringLiteral("mock"),
        QStringLiteral("--socket"),
        endpoint,
        QStringLiteral("--allow-unprivileged"),
        QStringLiteral("--exit-on-disconnect"),
    };
    if (!scenarioId.isEmpty()) {
        arguments.append(QStringLiteral("--mock-scenario"));
        arguments.append(scenarioId);
    }

    process.setProgram(daemonPath);
    process.setArguments(arguments);
    process.start();
    process.waitForStarted(2000);
}

bool readOnlyScenarioMatches(const QJsonObject& device, const QJsonArray& zones)
{
    return require(zones.size() == 2, "read-only scenario should expose two inventory zones")
        && require(device.value(QStringLiteral("readOnly")).toBool(false), "read-only scenario should serialize readOnly")
        && require(
            !device.value(QStringLiteral("writeRequiresConfirmation")).toBool(true),
            "read-only scenario should not report a confirmation-gated write path"
        );
}

bool confirmationScenarioMatches(const QJsonObject& device, const QJsonArray& zones)
{
    Q_UNUSED(zones)
    return require(
        device.value(QStringLiteral("writeRequiresConfirmation")).toBool(false),
        "confirmation-required scenario should serialize the write confirmation requirement"
    )
        && require(
            device.value(QStringLiteral("permission")).toObject().value(QStringLiteral("status")).toString()
                == QStringLiteral("requiresConfirmation"),
            "confirmation-required scenario should expose aggregate permission status"
        );
}

bool manyZonesScenarioMatches(const QJsonObject& device, const QJsonArray& zones)
{
    Q_UNUSED(device)
    return require(zones.size() == 16, "many-zones scenario should serialize sixteen zones")
        && require(
            zones.first().toObject().value(QStringLiteral("name")).toString() == QStringLiteral("Zone 01"),
            "many-zones scenario should keep stable first zone naming"
        )
        && require(
            zones.last().toObject().value(QStringLiteral("ledCount")).toInt() == 36,
            "many-zones scenario should expose an addressable stress zone last"
        );
}

bool failingWritesScenarioMatches(const QJsonObject& device, const QJsonArray& zones)
{
    return require(zones.size() == 1, "failing-writes scenario should expose one guarded zone")
        && require(
            !device.value(QStringLiteral("readOnly")).toBool(true),
            "failing-writes scenario should remain write-capable"
        )
        && require(
            !device.value(QStringLiteral("writeRequiresConfirmation")).toBool(true),
            "failing-writes scenario should not require confirmation before the simulated failure"
        )
        && require(
            device.value(QStringLiteral("permission")).toObject().value(QStringLiteral("status")).toString()
                == QStringLiteral("granted"),
            "failing-writes scenario should serialize granted aggregate permission"
        );
}

bool runDaemonVersionSmoke(const QString& daemonPath)
{
    QProcess daemon;
    daemon.setProgram(daemonPath);
    daemon.setArguments({QStringLiteral("--version")});
    daemon.start();
    if (!require(daemon.waitForFinished(3000), "daemon --version should exit promptly")) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return false;
    }

    const QString standardOutput = QString::fromLocal8Bit(daemon.readAllStandardOutput());
    return require(
        daemon.exitStatus() == QProcess::NormalExit && daemon.exitCode() == 0,
        "daemon --version should exit cleanly"
    )
        && require(
            standardOutput.contains(QStringLiteral("lumacore-daemon")),
            "daemon --version should identify the daemon binary"
        )
        && require(
            standardOutput.contains(lumacore::applicationVersion()),
            "daemon --version should report the application version"
        );
}

bool runMockScenarioInventorySmoke(
    const QString& daemonPath,
    const QString& scenarioId,
    const QString& expectedDeviceId,
    bool (*matches)(const QJsonObject& device, const QJsonArray& zones)
)
{
    QProcess daemon;
    const QString endpoint = uniqueEndpoint(QStringLiteral("scenario-%1").arg(scenarioId));
    startMockDaemon(daemon, daemonPath, endpoint, scenarioId);
    if (!require(daemon.state() != QProcess::NotRunning, "scenario daemon should start")) {
        return false;
    }

    auto client = std::make_shared<lumacore::DaemonClient>(endpoint);
    if (!require(waitForConnection(client), "scenario client should connect to the launched daemon")) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return false;
    }

    const lumacore::DaemonCallResult status =
        client->call(lumacore::daemonMethodName(lumacore::DaemonMethod::Status), {}, 3000);
    lumacore::DaemonCallResult devices;
    const bool inventoryReady = waitForCompleteInventory(client, &devices);
    const QJsonArray deviceArray = devices.result.value(QStringLiteral("devices")).toArray();
    if (!require(status.ok, "scenario status should succeed")
        || !require(
            status.result.value(QStringLiteral("backend")).toObject().value(QStringLiteral("id")).toString()
                == QStringLiteral("mock"),
            "scenario status should report the mock backend"
        )
        || !require(inventoryReady, "scenario discovery should complete")
        || !require(devices.ok, "scenario listDevices should succeed")
        || !require(deviceArray.size() == 1, "scenario listDevices should report one mock device")) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return false;
    }

    const QJsonObject device = deviceArray.first().toObject();
    const QJsonArray zones = device.value(QStringLiteral("zones")).toArray();
    const bool matched = require(
        device.value(QStringLiteral("id")).toString() == expectedDeviceId,
        "scenario mock device should expose the expected id"
    ) && matches(device, zones);

    client->disconnectFromDaemon();
    const bool exited = daemon.waitForFinished(3000);
    if (!exited) {
        daemon.kill();
        daemon.waitForFinished(2000);
    }
    return matched
        && require(exited, "scenario daemon should exit after the client disconnects")
        && require(
            daemon.exitStatus() == QProcess::NormalExit && daemon.exitCode() == 0,
            "scenario daemon should exit cleanly"
        );
}

bool runInvalidMockScenarioSmoke(const QString& daemonPath)
{
    QProcess daemon;
    daemon.setProgram(daemonPath);
    daemon.setArguments({
        QStringLiteral("--backend"),
        QStringLiteral("mock"),
        QStringLiteral("--socket"),
        uniqueEndpoint(QStringLiteral("invalid-scenario")),
        QStringLiteral("--allow-unprivileged"),
        QStringLiteral("--mock-scenario"),
        QStringLiteral("not-a-scenario"),
    });
    daemon.start();
    if (!require(daemon.waitForFinished(3000), "invalid scenario daemon should exit promptly")) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return false;
    }

    const QString standardError = QString::fromLocal8Bit(daemon.readAllStandardError());
    return require(
        daemon.exitStatus() == QProcess::NormalExit && daemon.exitCode() != 0,
        "invalid scenario daemon should fail startup"
    )
        && require(
            standardError.contains(QStringLiteral("Unknown mock scenario")),
            "invalid scenario daemon should explain the rejected scenario"
        );
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace lumacore;

    QCoreApplication application(argc, argv);
    if (!require(argc == 2, "end-to-end test expects the daemon binary path")) {
        return 1;
    }

    const QString daemonPath = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    if (!runDaemonVersionSmoke(daemonPath)
        || !runMockScenarioInventorySmoke(
            daemonPath,
            QStringLiteral("read-only"),
            QStringLiteral("mock-read-only-inventory"),
            readOnlyScenarioMatches
        )
        || !runMockScenarioInventorySmoke(
            daemonPath,
            QStringLiteral("confirmation-required"),
            QStringLiteral("mock-confirmation-required-controller"),
            confirmationScenarioMatches
        )
        || !runMockScenarioInventorySmoke(
            daemonPath,
            QStringLiteral("many-zones"),
            QStringLiteral("mock-many-zone-controller"),
            manyZonesScenarioMatches
        )
        || !runMockScenarioInventorySmoke(
            daemonPath,
            QStringLiteral("failing-writes"),
            QStringLiteral("mock-failing-writes-controller"),
            failingWritesScenarioMatches
        )
        || !runInvalidMockScenarioSmoke(daemonPath)) {
        return 1;
    }

    const QString endpoint = uniqueEndpoint(QStringLiteral("flow"));

    QProcess daemon;
    startMockDaemon(daemon, daemonPath, endpoint);
    if (!require(daemon.state() != QProcess::NotRunning, "mock daemon should start")) {
        return 1;
    }

    auto client = std::make_shared<DaemonClient>(endpoint);
    if (!require(waitForConnection(client), "client should connect to the launched daemon")) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Handshake: the daemon should report a matching protocol version, the mock
    // backend identity, and exactly one simulated device.
    const DaemonCallResult status = client->call(daemonMethodName(DaemonMethod::Status), {}, 3000);
    if (!require(status.ok, "status handshake should succeed")
        || !require(
            status.result.value(QStringLiteral("protocolVersion")).toInt() == kDaemonProtocolVersion,
            "daemon should advertise the shared protocol version"
        )
        || !require(
            client->protocolCompatible() && client->daemonProtocolVersion() == kDaemonProtocolVersion,
            "client should record a compatible daemon protocol version"
        )
        || !require(
            status.result.value(QStringLiteral("deviceCount")).toInt() == 1,
            "mock daemon should expose exactly one simulated device"
        )
        || !require(
            status.result.value(QStringLiteral("backend")).toObject().value(QStringLiteral("id")).toString()
                == QStringLiteral("mock"),
            "status should report the mock backend as active"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Inventory: the mock ASUS board and its three zones should round-trip.
    DaemonCallResult devices;
    const bool inventoryReady = waitForCompleteInventory(client, &devices);
    const QJsonArray deviceArray = devices.result.value(QStringLiteral("devices")).toArray();
    if (!require(inventoryReady, "device discovery should complete")
        || !require(devices.ok, "listDevices should succeed")
        || !require(deviceArray.size() == 1, "listDevices should report the single mock device")) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }
    const QJsonObject device = deviceArray.first().toObject();
    const QJsonArray zones = device.value(QStringLiteral("zones")).toArray();
    if (!require(
            device.value(QStringLiteral("id")).toString() == QStringLiteral("mock-asus-tuf-x870-plus-wifi"),
            "mock device should expose its stable id"
        )
        || !require(
            device.value(QStringLiteral("name")).toString() == QStringLiteral("Mock ASUS TUF X870-PLUS WIFI"),
            "mock device should expose its display name"
        )
        || !require(zones.size() == 3, "mock device should expose three zones")
        || !require(
            zones.at(0).toObject().value(QStringLiteral("name")).toString() == QStringLiteral("Header 1"),
            "first mock zone should be Header 1"
        )
        || !require(
            zones.at(1).toObject().value(QStringLiteral("ledCount")).toInt() == 30,
            "addressable mock zone should report its LED count"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Force dry-run on so the write assertions below are independent of the
    // platform default and never touch hardware.
    const DaemonCallResult dryRunOn = client->call(
        daemonMethodName(DaemonMethod::SetDryRun),
        {{QStringLiteral("enabled"), true}},
        3000
    );
    if (!require(dryRunOn.ok, "setDryRun should succeed")
        || !require(
            dryRunOn.result.value(QStringLiteral("dryRunEnabled")).toBool(false),
            "daemon should echo the enabled dry-run state"
        )
        || !require(
            client->daemonDryRunKnown() && client->daemonDryRunEnabled(),
            "client should learn the daemon dry-run state"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Dry-run static effect on the first zone: accepted without a hardware write.
    const QJsonObject staticEffect = RgbEffect(RgbEffectType::Static, RgbColor(30, 84, 214)).toJson();
    const DaemonCallResult applyEffect = client->call(
        daemonMethodName(DaemonMethod::ApplyEffect),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("zoneIndex"), 0},
            {QStringLiteral("effect"), staticEffect},
            {QStringLiteral("dryRunEnabled"), true},
        },
        3000
    );
    if (!require(applyEffect.ok, "applyEffect call should complete")
        || !require(
            applyEffect.result.value(QStringLiteral("success")).toBool(false),
            "dry-run applyEffect should be accepted"
        )
        || !require(
            applyEffect.result.value(QStringLiteral("hardwareStatus")).toString().contains(QStringLiteral("Dry-run accepted")),
            "dry-run applyEffect should not report a real hardware write"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Dry-run host-streamed frame on the addressable zone: exercises the
    // paintZoneFrame streaming path end to end.
    QVector<RgbColor> frame;
    frame.reserve(30);
    for (int index = 0; index < 30; ++index) {
        frame.append(RgbColor(index * 8, 255 - index * 8, 128));
    }
    const DaemonCallResult paintFrame = client->call(
        daemonMethodName(DaemonMethod::PaintZoneFrame),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("zoneIndex"), 1},
            {QStringLiteral("colors"), colorsToJson(frame)},
            {QStringLiteral("dryRunEnabled"), true},
        },
        3000
    );
    if (!require(paintFrame.ok, "paintZoneFrame call should complete")
        || !require(
            paintFrame.result.value(QStringLiteral("success")).toBool(false),
            "dry-run paintZoneFrame should be accepted"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Dry-run Wave effect on the addressable zone: the daemon must accept the
    // appended host-streamed effect types over the same applyEffect method.
    const QJsonObject waveEffect = RgbEffect(RgbEffectType::Wave, RgbColor(255, 40, 0), 2.0, 80).toJson();
    const DaemonCallResult applyWave = client->call(
        daemonMethodName(DaemonMethod::ApplyEffect),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("zoneIndex"), 1},
            {QStringLiteral("effect"), waveEffect},
            {QStringLiteral("dryRunEnabled"), true},
        },
        3000
    );
    if (!require(applyWave.ok, "applyEffect Wave call should complete")
        || !require(
            applyWave.result.value(QStringLiteral("success")).toBool(false),
            "dry-run Wave applyEffect should be accepted"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // The daemon dry-run synchronization guard should refuse a write whose
    // stated expectation drifts from the daemon's own dry-run state.
    const DaemonCallResult mismatchedApply = client->call(
        daemonMethodName(DaemonMethod::ApplyEffect),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("zoneIndex"), 0},
            {QStringLiteral("effect"), staticEffect},
            {QStringLiteral("dryRunEnabled"), false},
        },
        3000
    );
    if (!require(mismatchedApply.ok, "mismatched applyEffect call should complete")
        || !require(
            !mismatchedApply.result.value(QStringLiteral("success")).toBool(true),
            "the daemon should refuse a write whose dry-run expectation drifts"
        )
        || !require(
            mismatchedApply.result.value(QStringLiteral("error")).toString().contains(QStringLiteral("dry-run")),
            "a refused write should report the dry-run synchronization guard"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Dry-run All Off across the device.
    const DaemonCallResult allOff = client->call(
        daemonMethodName(DaemonMethod::AllOff),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("dryRunEnabled"), true},
        },
        3000
    );
    if (!require(allOff.ok, "allOff call should complete")
        || !require(
            allOff.result.value(QStringLiteral("success")).toBool(false),
            "dry-run allOff should be accepted"
        )
        || !require(
            allOff.result.value(QStringLiteral("hardwareStatus")).toString().contains(QStringLiteral("Dry-run accepted")),
            "dry-run allOff should not report a real hardware write"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Schedule support: the real daemon advertises scheduleSupported, accepts a
    // pushed profile, and echoes the normalized schedule back with availability.
    if (!require(
            status.result.value(QStringLiteral("scheduleSupported")).toBool(false),
            "real daemon should advertise schedule support in its status payload"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }
    const QJsonObject scheduleProfile {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("name"), QStringLiteral("E2E Schedule")},
        {QStringLiteral("devices"), QJsonArray {}},
    };
    const DaemonCallResult putProfile = client->call(
        daemonMethodName(DaemonMethod::PutProfile),
        {
            {QStringLiteral("profileName"), QStringLiteral("E2E Schedule")},
            {QStringLiteral("profile"), scheduleProfile},
        },
        3000
    );
    const DaemonCallResult setSchedule = client->call(
        daemonMethodName(DaemonMethod::SetSchedule),
        {
            {QStringLiteral("enabled"), true},
            {QStringLiteral("profileName"), QStringLiteral("E2E Schedule")},
            {QStringLiteral("time"), QStringLiteral("07:30")},
        },
        3000
    );
    const DaemonCallResult getSchedule = client->call(daemonMethodName(DaemonMethod::GetSchedule), {}, 3000);
    const DaemonCallResult disableSchedule = client->call(
        daemonMethodName(DaemonMethod::SetSchedule),
        {
            {QStringLiteral("enabled"), false},
            {QStringLiteral("profileName"), QString()},
            {QStringLiteral("time"), QStringLiteral("18:00")},
        },
        3000
    );
    if (!require(
            putProfile.ok && putProfile.result.value(QStringLiteral("success")).toBool(false),
            "real daemon should accept a pushed schedule profile"
        )
        || !require(
            setSchedule.ok
                && setSchedule.result.value(QStringLiteral("success")).toBool(false)
                && setSchedule.result.value(QStringLiteral("enabled")).toBool(false)
                && setSchedule.result.value(QStringLiteral("time")).toString() == QStringLiteral("07:30"),
            "real daemon should accept and echo a valid schedule"
        )
        || !require(
            getSchedule.ok
                && getSchedule.result.value(QStringLiteral("enabled")).toBool(false)
                && getSchedule.result.value(QStringLiteral("profileName")).toString()
                    == QStringLiteral("E2E Schedule")
                && getSchedule.result.value(QStringLiteral("profileAvailable")).toBool(false),
            "real daemon should report the stored schedule with the pushed profile available"
        )
        || !require(
            disableSchedule.ok && disableSchedule.result.value(QStringLiteral("success")).toBool(false),
            "schedule cleanup should disable the end-to-end schedule"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // The daemon logs its startup banner, so a snapshot should never be empty.
    const DaemonCallResult activity = client->call(daemonMethodName(DaemonMethod::ActivityLogSnapshot), {}, 3000);
    if (!require(activity.ok, "activityLogSnapshot should succeed")
        || !require(
            !activity.result.value(QStringLiteral("lines")).toArray().isEmpty(),
            "daemon activity log should carry at least its startup entry"
        )) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }

    // Idle shutdown: --exit-on-disconnect should stop the daemon once the only
    // client disconnects.
    client->disconnectFromDaemon();
    if (!require(daemon.waitForFinished(3000), "idle daemon should exit after the client disconnects")) {
        daemon.kill();
        daemon.waitForFinished(2000);
        return 1;
    }
    if (!require(
            daemon.exitStatus() == QProcess::NormalExit && daemon.exitCode() == 0,
            "daemon should exit cleanly"
        )) {
        return 1;
    }

    return 0;
}
