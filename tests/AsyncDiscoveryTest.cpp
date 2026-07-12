// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/mock/MockRgbDevice.h"
#include "core/DeviceManager.h"
#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QThread>

#include <cstdio>
#include <functional>
#include <memory>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (condition()) {
            return true;
        }
        QThread::msleep(5);
    }
    return condition();
}

QString uniqueEndpoint()
{
#ifdef Q_OS_WIN
    return QStringLiteral("lumacore-async-discovery-%1").arg(QCoreApplication::applicationPid());
#else
    return QDir::temp().filePath(
        QStringLiteral("lumacore-async-discovery-%1.sock").arg(QCoreApplication::applicationPid())
    );
#endif
}

class SlowBackend final : public lumacore::RgbBackend
{
public:
    explicit SlowBackend(unsigned long delayMs)
        : m_delayMs(delayMs)
    {
    }

    [[nodiscard]] lumacore::BackendDescriptor descriptor() const override
    {
        return {
            QStringLiteral("slow"),
            QStringLiteral("Slow test backend"),
            QStringLiteral("Deterministic asynchronous discovery test backend."),
            lumacore::BackendCapability::DiscoveryRead,
        };
    }

    [[nodiscard]] std::vector<std::unique_ptr<lumacore::RgbDevice>> discoverDevices() const override
    {
        QThread::msleep(m_delayMs);
        std::vector<std::unique_ptr<lumacore::RgbDevice>> devices;
        devices.push_back(std::make_unique<lumacore::MockRgbDevice>());
        return devices;
    }

private:
    unsigned long m_delayMs;
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    lumacore::DeviceManager deviceManager;
    deviceManager.registerBackend(std::make_unique<SlowBackend>(500));

    lumacore::DaemonServer server(&deviceManager);
    QString listenError;
    if (!require(server.listen(uniqueEndpoint(), &listenError), "daemon server should listen before discovery")
        || !require(
            deviceManager.startBackendDiscovery(QStringLiteral("slow")),
            "asynchronous discovery should start"
        )
        || !require(deviceManager.discoveryInProgress(), "discovery should report in progress")) {
        return 1;
    }

    auto client = std::make_shared<lumacore::DaemonClient>(server.socketPath());
    if (!require(client->connectToDaemon(1000), "client should connect while discovery is running")) {
        return 1;
    }

    const lumacore::DaemonCallResult hello =
        client->call(lumacore::daemonMethodName(lumacore::DaemonMethod::Hello), {}, 1000);
    const lumacore::DaemonCallResult status =
        client->call(lumacore::daemonMethodName(lumacore::DaemonMethod::Status), {}, 1000);
    const lumacore::DaemonCallResult incomplete =
        client->call(lumacore::daemonMethodName(lumacore::DaemonMethod::ListDevices), {}, 1000);
    const lumacore::DaemonCallResult rejectedUpdate =
        client->call(lumacore::daemonMethodName(lumacore::DaemonMethod::UpdateZone), {}, 1000);
    if (!require(
            hello.ok && !hello.result.value(QStringLiteral("discoveryComplete")).toBool(true),
            "hello should expose incomplete discovery"
        )
        || !require(
            status.ok && !status.result.value(QStringLiteral("discoveryComplete")).toBool(true),
            "status should expose incomplete discovery"
        )
        || !require(incomplete.ok, "in-progress listDevices should succeed")
        || !require(
            !incomplete.result.value(QStringLiteral("discoveryComplete")).toBool(true),
            "in-progress inventory should report discovery incomplete"
        )
        || !require(
            incomplete.result.value(QStringLiteral("devices")).toArray().isEmpty(),
            "in-progress inventory should be empty"
        )
        || !require(!rejectedUpdate.ok, "device methods should be rejected during discovery")
        || !require(
            rejectedUpdate.error == QStringLiteral("Device discovery is still in progress."),
            "device method rejection should explain discovery state"
        )) {
        return 1;
    }

    if (!require(
            waitUntil([&deviceManager] { return deviceManager.discoveryComplete(); }),
            "asynchronous discovery should complete"
        )
        || !require(deviceManager.deviceCount() == 1, "completed discovery should install one device")
        || !require(
            deviceManager.deviceAt(0)->thread() == deviceManager.thread(),
            "worker-created device should move to the manager thread"
        )) {
        return 1;
    }

    const lumacore::DaemonCallResult complete =
        client->call(lumacore::daemonMethodName(lumacore::DaemonMethod::ListDevices), {}, 1000);
    if (!require(complete.ok, "completed listDevices should succeed")
        || !require(
            complete.result.value(QStringLiteral("discoveryComplete")).toBool(false),
            "completed inventory should report discovery complete"
        )
        || !require(
            complete.result.value(QStringLiteral("devices")).toArray().size() == 1,
            "completed inventory should contain the discovered device"
        )) {
        return 1;
    }

    client->disconnectFromDaemon();
    server.close();

    {
        lumacore::DeviceManager shuttingDownManager;
        shuttingDownManager.registerBackend(std::make_unique<SlowBackend>(50));
        if (!require(
                shuttingDownManager.startBackendDiscovery(QStringLiteral("slow")),
                "shutdown discovery should start"
            )) {
            return 1;
        }
    }

    return 0;
}
