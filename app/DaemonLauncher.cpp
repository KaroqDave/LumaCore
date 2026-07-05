// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/DaemonLauncher.h"

#include "ipc/DaemonClient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <QThread>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace lumacore {

DaemonLauncher::DaemonLauncher(std::shared_ptr<DaemonClient> client)
    : m_client(std::move(client))
{
    m_process.setStandardOutputFile(QProcess::nullDevice());
    m_process.setStandardErrorFile(QProcess::nullDevice());
#ifdef Q_OS_WIN
    m_process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#endif
}

DaemonLauncher::~DaemonLauncher()
{
    stopStartedProcess();
}

bool DaemonLauncher::ensureAvailable(
    bool autoStart,
    const QString& daemonExecutable,
    int startupTimeoutMs,
    std::optional<bool> initialDryRun
)
{
    m_lastError.clear();
    m_startedDaemon = false;

    if (m_client == nullptr) {
        m_lastError = QStringLiteral("Daemon client is not available.");
        return false;
    }

    if (m_client->connectToDaemon(150)) {
        return true;
    }

    if (!autoStart) {
        m_lastError = m_client->lastError();
        return false;
    }

    const QString executable = resolvedDaemonExecutable(daemonExecutable);
    if (!QFileInfo::exists(executable)) {
        m_lastError = QStringLiteral(
            "Could not start the LumaCore daemon because '%1' was not found. "
            "Re-extract the complete Windows package or start the daemon manually."
        ).arg(QDir::toNativeSeparators(executable));
        m_client->reportConnectionError(m_lastError);
        return false;
    }

    QStringList arguments {
        QStringLiteral("--backend"),
        QStringLiteral("auto"),
        QStringLiteral("--socket"),
        m_client->socketPath(),
        QStringLiteral("--exit-on-disconnect"),
    };
    if (initialDryRun.has_value()) {
        arguments << QStringLiteral("--dry-run")
                  << (*initialDryRun ? QStringLiteral("true") : QStringLiteral("false"));
    }
    m_process.setProgram(executable);
    m_process.setArguments(arguments);
    m_process.start();
    if (!m_process.waitForStarted(1000)) {
        m_lastError = QStringLiteral("Could not start the bundled LumaCore daemon: %1")
                          .arg(m_process.errorString());
        m_client->reportConnectionError(m_lastError);
        return false;
    }
    m_startedDaemon = true;

    QElapsedTimer timer;
    timer.start();
    bool processExitedBeforeConnection = false;
    int processExitCode = 0;
    while (timer.elapsed() < startupTimeoutMs) {
        if (m_client->connectToDaemon(100)) {
            return true;
        }
        if (!processExitedBeforeConnection && m_process.state() == QProcess::NotRunning) {
            processExitedBeforeConnection = true;
            processExitCode = m_process.exitCode();
            m_startedDaemon = false;
        }
        QCoreApplication::processEvents();
        QThread::msleep(25);
    }

    if (processExitedBeforeConnection) {
        m_lastError = QStringLiteral(
            "The bundled LumaCore daemon exited before it accepted a connection (exit code %1)."
        ).arg(processExitCode);
    } else {
        m_lastError = QStringLiteral(
            "Timed out waiting for the bundled LumaCore daemon at %1. "
            "You can retry by restarting LumaCore or start the daemon manually."
        ).arg(m_client->socketPath());
    }
    m_client->reportConnectionError(m_lastError);
    stopStartedProcess();
    return false;
}

void DaemonLauncher::ensureAvailableAsync(
    bool autoStart,
    const QString& daemonExecutable,
    std::optional<bool> initialDryRun
)
{
    m_lastError.clear();
    m_startedDaemon = false;
    m_startedDaemonSawConnection = false;

    if (m_client == nullptr) {
        m_lastError = QStringLiteral("Daemon client is not available.");
        return;
    }
    if (m_client->isConnected() || !autoStart) {
        return;
    }

    const QString executable = resolvedDaemonExecutable(daemonExecutable);
    if (!QFileInfo::exists(executable)) {
        m_lastError = QStringLiteral(
            "Could not start the LumaCore daemon because '%1' was not found. "
            "Re-extract the complete Windows package or start the daemon manually."
        ).arg(QDir::toNativeSeparators(executable));
        m_client->reportConnectionError(m_lastError);
        return;
    }

    QStringList arguments {
        QStringLiteral("--backend"),
        QStringLiteral("auto"),
        QStringLiteral("--socket"),
        m_client->socketPath(),
        QStringLiteral("--exit-on-disconnect"),
    };
    if (initialDryRun.has_value()) {
        arguments << QStringLiteral("--dry-run")
                  << (*initialDryRun ? QStringLiteral("true") : QStringLiteral("false"));
    }
    m_process.setProgram(executable);
    m_process.setArguments(arguments);
    m_process.start();
    if (!m_process.waitForStarted(1000)) {
        m_lastError = QStringLiteral("Could not start the bundled LumaCore daemon: %1")
                          .arg(m_process.errorString());
        m_client->reportConnectionError(m_lastError);
        return;
    }
    m_startedDaemon = true;

    // Surface a startup crash distinctly from a plain connection timeout: if
    // the launched process exits before the client ever connected, report its
    // exit code. Both connections use the client as context so they detach
    // with it.
    QObject::connect(
        m_client.get(),
        &DaemonClient::connectionStateChanged,
        m_client.get(),
        [this] {
            if (m_client->isConnected()) {
                m_startedDaemonSawConnection = true;
            }
        }
    );
    QObject::connect(
        &m_process,
        &QProcess::finished,
        m_client.get(),
        [this](int exitCode, QProcess::ExitStatus) {
            if (m_startedDaemonSawConnection) {
                return;
            }
            m_startedDaemon = false;
            m_lastError = QStringLiteral(
                "The bundled LumaCore daemon exited before it accepted a connection (exit code %1)."
            ).arg(exitCode);
            m_client->reportConnectionError(m_lastError);
        }
    );
}

bool DaemonLauncher::startedDaemon() const
{
    return m_startedDaemon;
}

QString DaemonLauncher::lastError() const
{
    return m_lastError;
}

bool DaemonLauncher::waitForStartedDaemonExit(int timeoutMs)
{
    return !m_startedDaemon
        || m_process.state() == QProcess::NotRunning
        || m_process.waitForFinished(timeoutMs);
}

QString DaemonLauncher::resolvedDaemonExecutable(const QString& overridePath) const
{
    if (!overridePath.isEmpty()) {
        return QFileInfo(overridePath).absoluteFilePath();
    }

#ifdef Q_OS_WIN
    constexpr auto executableName = "lumacore-daemon.exe";
#else
    constexpr auto executableName = "lumacore-daemon";
#endif
    return QCoreApplication::applicationDirPath() + QLatin1Char('/') + QString::fromLatin1(executableName);
}

void DaemonLauncher::stopStartedProcess()
{
    if (!m_startedDaemon || m_process.state() == QProcess::NotRunning) {
        return;
    }

    if (m_client != nullptr) {
        m_client->disconnectFromDaemon();
    }
    if (m_process.waitForFinished(1000)) {
        return;
    }

    m_process.terminate();
    if (!m_process.waitForFinished(500)) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
}

} // namespace lumacore
