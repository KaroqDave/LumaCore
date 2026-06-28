// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/DaemonLauncher.h"
#include "ipc/DaemonClient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QThread>

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
    if (!require(argc == 4, "launcher test expects daemon, exit fixture, and sleep fixture paths")) {
        return 1;
    }

    const QString daemonPath = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString exitFixturePath = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    const QString sleepFixturePath = QFileInfo(QString::fromLocal8Bit(argv[3])).absoluteFilePath();

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
        startDaemon(externalDaemon, daemonPath, endpoint);
        auto client = std::make_shared<lumacore::DaemonClient>(endpoint);
        if (!require(externalDaemon.state() != QProcess::NotRunning, "external daemon should start")
            || !require(waitForConnection(client), "external daemon should accept a connection")) {
            return 1;
        }

        lumacore::DaemonLauncher launcher(client);
        if (!require(
                launcher.ensureAvailable(true, QDir::temp().filePath(QStringLiteral("must-not-be-started.exe"))),
                "launcher should reuse an existing daemon"
            )
            || !require(!launcher.startedDaemon(), "reused daemon should not be marked as launcher-owned")) {
            return 1;
        }

        const lumacore::DaemonCallResult devices = client->call(QStringLiteral("listDevices"));
        if (!require(devices.ok, "reused daemon should answer listDevices")
            || !require(!devices.result.value(QStringLiteral("devices")).toArray().isEmpty(), "mock daemon should expose a device")) {
            return 1;
        }
        client->disconnectFromDaemon();
        if (!require(externalDaemon.waitForFinished(3000), "external idle daemon should exit after disconnect")) {
            externalDaemon.kill();
            return 1;
        }
    }

#ifdef Q_OS_WIN
    {
        const QString endpoint = uniqueEndpoint(QStringLiteral("owned"));
        auto client = std::make_shared<lumacore::DaemonClient>(endpoint);
        lumacore::DaemonLauncher launcher(client);
        if (!require(launcher.ensureAvailable(true, daemonPath), "launcher should start the bundled daemon")
            || !require(launcher.startedDaemon(), "started daemon should be marked as launcher-owned")) {
            return 1;
        }

        const lumacore::DaemonCallResult devices = client->call(QStringLiteral("listDevices"));
        if (!require(devices.ok, "launcher-owned daemon should answer listDevices")
            || !require(!devices.result.value(QStringLiteral("devices")).toArray().isEmpty(), "launcher-owned auto daemon should expose inventory or mock fallback")) {
            return 1;
        }
        client->disconnectFromDaemon();
        if (!require(launcher.waitForStartedDaemonExit(3000), "launcher-owned daemon should exit when its client disconnects")) {
            return 1;
        }
    }
#endif

    return 0;
}
