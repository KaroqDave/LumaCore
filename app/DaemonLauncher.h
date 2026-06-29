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
        std::optional<bool> initialDryRun = std::nullopt
    );
    [[nodiscard]] bool startedDaemon() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool waitForStartedDaemonExit(int timeoutMs);

private:
    [[nodiscard]] QString resolvedDaemonExecutable(const QString& overridePath) const;
    void stopStartedProcess();

    std::shared_ptr<DaemonClient> m_client;
    QProcess m_process;
    QString m_lastError;
    bool m_startedDaemon {false};
};

} // namespace lumacore
