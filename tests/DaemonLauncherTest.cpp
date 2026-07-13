// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/DaemonLauncher.h"
#include "ipc/DaemonClient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QThread>

#include <functional>
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
    return QStringLiteral("lumacore-launcher-%1-%2")
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

// Event-loop-driven wait for the asynchronous launcher cases, where the
// client's own reconnect machinery (timers and socket signals) must run.
bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return condition();
}

bool waitForCompleteInventory(
    const std::shared_ptr<lumacore::DaemonClient>& client,
    lumacore::DaemonCallResult* result,
    int timeoutMs = 5000,
    bool* sawIncomplete = nullptr
)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const lumacore::DaemonCallResult response = client->call(
            QStringLiteral("listDevices"),
            {{QStringLiteral("acceptIncompleteDiscovery"), true}}
        );
        if (sawIncomplete != nullptr
            && response.ok
            && !response.result.value(QStringLiteral("discoveryComplete")).toBool(true)) {
            *sawIncomplete = true;
        }
        if (response.ok
            && response.result.value(QStringLiteral("discoveryComplete")).toBool(true)) {
            if (result != nullptr) {
                *result = response;
            }
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(25);
    }
    return false;
}

void startDaemon(QProcess& process, const QString& daemonPath, const QString& endpoint)
{
    process.setProgram(daemonPath);
    process.setArguments({
        QStringLiteral("--backend"),
        QStringLiteral("mock"),
        QStringLiteral("--socket"),
        endpoint,
        QStringLiteral("--allow-unprivileged"),
        QStringLiteral("--exit-on-disconnect"),
    });
    process.start();
    process.waitForStarted(2000);
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    if (!require(argc == 5, "launcher test expects daemon, exit fixture, sleep fixture, and scenario fixture paths")) {
        return 1;
    }

    const QString daemonPath = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString exitFixturePath = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    const QString sleepFixturePath = QFileInfo(QString::fromLocal8Bit(argv[3])).absoluteFilePath();
    const QString scenarioFixturePath = QFileInfo(QString::fromLocal8Bit(argv[4])).absoluteFilePath();

    {
        auto client = std::make_shared<lumacore::DaemonClient>(uniqueEndpoint(QStringLiteral("missing")));
        lumacore::DaemonLauncher launcher(client);
        const QString missingPath = QDir::temp().filePath(QStringLiteral("missing-lumacore-daemon.exe"));
        if (!require(!launcher.ensureAvailable(true, missingPath, 250), "missing daemon should fail")
            || !require(launcher.lastError().contains(QStringLiteral("not found")), "missing daemon should report an actionable error")) {
            return 1;
        }
    }

    {
        auto client = std::make_shared<lumacore::DaemonClient>(uniqueEndpoint(QStringLiteral("early-exit")));
        lumacore::DaemonLauncher launcher(client);
        if (!require(!launcher.ensureAvailable(true, exitFixturePath, 1000), "early daemon exit should fail")
            || !require(launcher.lastError().contains(QStringLiteral("exited before")), "early exit should be reported")) {
            return 1;
        }
    }

    {
        auto client = std::make_shared<lumacore::DaemonClient>(uniqueEndpoint(QStringLiteral("timeout")));
        lumacore::DaemonLauncher launcher(client);
        if (!require(!launcher.ensureAvailable(true, sleepFixturePath, 250), "non-listening daemon should time out")
            || !require(launcher.lastError().contains(QStringLiteral("Timed out")), "startup timeout should be reported")) {
            return 1;
        }
    }

    {
        const QString endpoint = uniqueEndpoint(QStringLiteral("reuse"));
        QProcess externalDaemon;
        QElapsedTimer externalStartupTimer;
        externalStartupTimer.start();
        startDaemon(externalDaemon, daemonPath, endpoint);
        auto client = std::make_shared<lumacore::DaemonClient>(endpoint);
        if (!require(externalDaemon.state() != QProcess::NotRunning, "external daemon should start")
            || !require(waitForConnection(client), "external daemon should accept a connection")) {
            return 1;
        }
        const qint64 launchToConnectMs = externalStartupTimer.elapsed();

        lumacore::DaemonLauncher launcher(client);
        if (!require(
                launcher.ensureAvailable(true, QDir::temp().filePath(QStringLiteral("must-not-be-started.exe"))),
                "launcher should reuse an existing daemon"
            )
            || !require(!launcher.startedDaemon(), "reused daemon should not be marked as launcher-owned")) {
            return 1;
        }

        QElapsedTimer inventoryTimer;
        inventoryTimer.start();
        lumacore::DaemonCallResult devices;
        bool sawIncomplete = false;
        const bool inventoryReady = waitForCompleteInventory(client, &devices, 5000, &sawIncomplete);
        const qint64 connectionToInventoryMs = inventoryTimer.elapsed();
        if (!require(inventoryReady, "reused daemon should finish device discovery")
            || !require(devices.ok, "reused daemon should answer listDevices")
            || !require(!devices.result.value(QStringLiteral("devices")).toArray().isEmpty(), "mock daemon should expose a device")) {
            return 1;
        }
        std::fprintf(
            stdout,
            "EXTERNAL_STARTUP_TIMING launch_to_connect_ms=%lld connection_to_inventory_ms=%lld "
            "observed_incomplete=%d\n",
            static_cast<long long>(launchToConnectMs),
            static_cast<long long>(connectionToInventoryMs),
            sawIncomplete ? 1 : 0
        );
        std::fflush(stdout);
        client->disconnectFromDaemon();
        if (!require(externalDaemon.waitForFinished(3000), "external idle daemon should exit after disconnect")) {
            externalDaemon.kill();
            return 1;
        }
    }

    {
        // Asynchronous launch failure: a missing executable reports through
        // the client immediately without blocking.
        auto client = std::make_shared<lumacore::DaemonClient>(uniqueEndpoint(QStringLiteral("async-missing")));
        lumacore::DaemonLauncher launcher(client);
        launcher.ensureAvailableAsync(true, QDir::temp().filePath(QStringLiteral("missing-lumacore-daemon.exe")));
        if (!require(launcher.lastError().contains(QStringLiteral("not found")), "async missing daemon should report an actionable error")
            || !require(client->lastError().contains(QStringLiteral("not found")), "async missing daemon should surface the error through the client")) {
            return 1;
        }
    }

    {
        // Asynchronous launch with an immediately-exiting daemon: the exit is
        // reported with its code once the event loop delivers it.
        auto client = std::make_shared<lumacore::DaemonClient>(uniqueEndpoint(QStringLiteral("async-early-exit")));
        lumacore::DaemonLauncher launcher(client);
        launcher.ensureAvailableAsync(true, exitFixturePath);
        if (!require(
                waitUntil([&launcher] { return launcher.lastError().contains(QStringLiteral("exited before")); }),
                "async early daemon exit should be reported"
            )
            || !require(!launcher.startedDaemon(), "an exited daemon should not stay marked as launcher-owned")) {
            return 1;
        }
    }

#ifdef Q_OS_WIN
    {
        // Asynchronous launch happy path: the launcher starts the daemon
        // without blocking and the client's reconnect machinery connects.
        const QString endpoint = uniqueEndpoint(QStringLiteral("async-owned"));
        auto client = std::make_shared<lumacore::DaemonClient>(endpoint);
        lumacore::DaemonLauncher launcher(client);
        QList<int> reconnectDelays;
        QObject::connect(
            client.get(),
            &lumacore::DaemonClient::reconnectScheduled,
            [&reconnectDelays](int, int delayMs) { reconnectDelays.append(delayMs); }
        );
        QElapsedTimer startupTimer;
        startupTimer.start();
        launcher.ensureAvailableAsync(true, daemonPath);
        if (!require(launcher.startedDaemon(), "async launcher should start the bundled daemon")) {
            return 1;
        }
        client->setAutomaticReconnectEnabled(true);
        if (!require(
                waitUntil([&client] { return client->isConnected(); }),
                "reconnect machinery should connect to the async-launched daemon"
            )) {
            return 1;
        }
        const qint64 launchToConnectMs = startupTimer.elapsed();
        QElapsedTimer inventoryTimer;
        inventoryTimer.start();
        lumacore::DaemonCallResult asyncDevices;
        bool sawIncomplete = false;
        const bool inventoryReady = waitForCompleteInventory(
            client,
            &asyncDevices,
            5000,
            &sawIncomplete
        );
        const qint64 connectionToInventoryMs = inventoryTimer.elapsed();
        if (!require(inventoryReady, "async-launched daemon should finish device discovery")
            || !require(asyncDevices.ok, "async-launched daemon should answer listDevices")
            || !require(
                !asyncDevices.result.value(QStringLiteral("devices")).toArray().isEmpty(),
                "async-launched daemon should expose its final device inventory"
            )
            || !require(!reconnectDelays.isEmpty(), "async launch should record a reconnect attempt")
            || !require(
                reconnectDelays.constFirst() == 0,
                "async launch should attempt its first daemon connection immediately"
            )) {
            return 1;
        }
        std::fprintf(
            stdout,
            "STARTUP_TIMING launch_to_connect_ms=%lld connection_to_inventory_ms=%lld "
            "first_reconnect_delay_ms=%d observed_incomplete=%d\n",
            static_cast<long long>(launchToConnectMs),
            static_cast<long long>(connectionToInventoryMs),
            reconnectDelays.constFirst(),
            sawIncomplete ? 1 : 0
        );
        std::fflush(stdout);
        client->disconnectFromDaemon();
        if (!require(launcher.waitForStartedDaemonExit(3000), "async-launched daemon should exit when its client disconnects")) {
            return 1;
        }
    }

    {
        const QString endpoint = uniqueEndpoint(QStringLiteral("owned"));
        auto client = std::make_shared<lumacore::DaemonClient>(endpoint);
        lumacore::DaemonLauncher launcher(client);
        if (!require(
                launcher.ensureAvailable(true, daemonPath, 3000),
                "launcher should start the bundled daemon"
            )
            || !require(launcher.startedDaemon(), "started daemon should be marked as launcher-owned")) {
            return 1;
        }

        lumacore::DaemonCallResult devices;
        const bool inventoryReady = waitForCompleteInventory(client, &devices);
        const QJsonArray deviceArray = devices.result.value(QStringLiteral("devices")).toArray();
        if (!require(inventoryReady, "launcher-owned daemon should finish device discovery")
            || !require(devices.ok, "launcher-owned daemon should answer listDevices")
            || !require(!deviceArray.isEmpty(), "launcher-owned auto daemon should expose devices")) {
            return 1;
        }
        client->disconnectFromDaemon();
        if (!require(launcher.waitForStartedDaemonExit(3000), "launcher-owned daemon should exit when its client disconnects")) {
            return 1;
        }
    }

    {
        auto client = std::make_shared<lumacore::DaemonClient>(uniqueEndpoint(QStringLiteral("scenario-args")));
        lumacore::DaemonLauncher launcher(client);
        if (!require(
                !launcher.ensureAvailable(true, scenarioFixturePath, 250, std::nullopt, QStringLiteral("many-zones")),
                "scenario fixture should exit after validating launcher arguments"
            )
            || !require(
                launcher.lastError().contains(QStringLiteral("exit code 42")),
                "launcher should pass the selected mock scenario to the daemon process"
            )) {
            return 1;
        }
    }
#endif

    return 0;
}
