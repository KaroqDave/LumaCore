// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "ipc/DaemonProtocol.h"

#include <QLocalSocket>
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QTimer>

#include <functional>

namespace lumacore {

struct DaemonCallResult {
    bool ok {false};
    QJsonObject result;
    QString error;
};

class DaemonClient final : public QObject
{
    Q_OBJECT

public:
    using CallHandler = std::function<void(DaemonCallResult)>;

    explicit DaemonClient(QString socketPath = defaultDaemonSocketPath(), QObject* parent = nullptr);

    [[nodiscard]] const QString& socketPath() const;
    void setSocketPath(QString socketPath);

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QString daemonVersion() const;
    [[nodiscard]] int daemonProtocolVersion() const;
    [[nodiscard]] bool daemonDryRunKnown() const;
    [[nodiscard]] bool daemonDryRunEnabled() const;
    [[nodiscard]] bool daemonScheduleSupported() const;
    [[nodiscard]] bool protocolCompatible() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool automaticReconnectEnabled() const;
    [[nodiscard]] int reconnectAttempt() const;
    [[nodiscard]] bool isConnecting() const;
    [[nodiscard]] bool isReconnectScheduled() const;

    [[nodiscard]] bool connectToDaemon(int timeoutMs = 1000);
    void disconnectFromDaemon();
    void setAutomaticReconnectEnabled(bool enabled);
    void reconnectNow();
    void reportConnectionError(const QString& message);
    [[nodiscard]] DaemonCallResult call(const QString& method, const QJsonObject& params = {}, int timeoutMs = 2000);
    [[nodiscard]] quint64 callAsync(
        const QString& method,
        const QJsonObject& params,
        CallHandler handler,
        int timeoutMs = 2000
    );
    [[nodiscard]] bool cancelCall(quint64 requestId);

signals:
    void connectionStateChanged();
    void lastErrorChanged();
    void daemonInfoChanged();
    void reconnectScheduled(int attempt, int delayMs);
    void reconnected();

private:
    struct PendingCall {
        QByteArray payload;
        CallHandler handler;
        QTimer* timeoutTimer {nullptr};
        bool written {false};
    };

    void setLastError(const QString& message);
    void setDaemonVersion(const QString& version);
    void setDaemonProtocolVersion(int version);
    void setDaemonDryRunEnabled(bool enabled);
    void clearDaemonDryRunState();
    void setDaemonScheduleSupported(bool supported);
    void updateDaemonInfo(const DaemonCallResult& result);
    [[nodiscard]] bool ensureConnected(int timeoutMs);
    void startAsyncConnection();
    void sendPendingCall(quint64 requestId);
    void sendPendingCalls();
    void readAsyncResponses();
    void finishPendingCall(quint64 requestId, DaemonCallResult result);
    void failPendingCalls(const QString& error);
    void scheduleReconnect();
    void resetReconnectState();

    QString m_socketPath;
    QLocalSocket m_socket;
    QByteArray m_buffer;
    quint64 m_nextRequestId {1};
    QHash<quint64, PendingCall> m_pendingCalls;
    QList<quint64> m_pendingCallOrder;
    QString m_daemonVersion;
    int m_daemonProtocolVersion {0};
    bool m_daemonDryRunKnown {false};
    bool m_daemonDryRunEnabled {false};
    bool m_daemonScheduleSupported {false};
    QString m_lastError;
    QTimer m_reconnectTimer;
    int m_reconnectAttempt {0};
    bool m_automaticReconnectEnabled {false};
    bool m_manualDisconnect {false};
};

} // namespace lumacore
