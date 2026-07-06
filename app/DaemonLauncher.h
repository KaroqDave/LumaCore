// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QProcess>
#include <QString>

#include <memory>
#include <optional>

namespace lumacore {

class DaemonClient;

class DaemonLauncher final
{
public:
    explicit DaemonLauncher(std::shared_ptr<DaemonClient> client);
    ~DaemonLauncher();

    DaemonLauncher(const DaemonLauncher&) = delete;
    DaemonLauncher& operator=(const DaemonLauncher&) = delete;

    [[nodiscard]] bool ensureAvailable(
        bool autoStart,
        const QString& daemonExecutable = {},
        int startupTimeoutMs = 3000,
        std::optional<bool> initialDryRun = std::nullopt,
        const QString& mockScenarioId = {}
    );
    // Start the bundled daemon (when autoStart is set) without blocking on the
    // connection: acquisition is owned by the client's reconnect machinery,
    // which the caller enables separately. Startup failures are reported
    // through DaemonClient::reportConnectionError.
    void ensureAvailableAsync(
        bool autoStart,
        const QString& daemonExecutable = {},
        std::optional<bool> initialDryRun = std::nullopt,
        const QString& mockScenarioId = {}
    );
    [[nodiscard]] bool startedDaemon() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool waitForStartedDaemonExit(int timeoutMs);

private:
    [[nodiscard]] QString resolvedDaemonExecutable(const QString& overridePath) const;
    [[nodiscard]] QStringList daemonArguments(std::optional<bool> initialDryRun, const QString& mockScenarioId) const;
    void stopStartedProcess();

    std::shared_ptr<DaemonClient> m_client;
    QProcess m_process;
    QString m_lastError;
    bool m_startedDaemon {false};
    bool m_startedDaemonSawConnection {false};
};

} // namespace lumacore
