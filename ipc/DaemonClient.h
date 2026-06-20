#pragma once

#include "ipc/DaemonProtocol.h"

#include <QLocalSocket>
#include <QObject>
#include <QJsonObject>
#include <QString>

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
    explicit DaemonClient(QString socketPath = defaultDaemonSocketPath(), QObject* parent = nullptr);

    [[nodiscard]] const QString& socketPath() const;
    void setSocketPath(QString socketPath);

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QString daemonVersion() const;
    [[nodiscard]] int daemonProtocolVersion() const;
    [[nodiscard]] bool protocolCompatible() const;
    [[nodiscard]] QString lastError() const;

    [[nodiscard]] bool connectToDaemon(int timeoutMs = 1000);
    void disconnectFromDaemon();
    void reportConnectionError(const QString& message);
    [[nodiscard]] DaemonCallResult call(const QString& method, const QJsonObject& params = {}, int timeoutMs = 2000);

signals:
    void connectionStateChanged();
    void lastErrorChanged();
    void daemonInfoChanged();

private:
    void setLastError(const QString& message);
    void setDaemonVersion(const QString& version);
    void setDaemonProtocolVersion(int version);
    [[nodiscard]] bool ensureConnected(int timeoutMs);
    [[nodiscard]] DaemonCallResult readResponse(quint64 requestId, int timeoutMs);

    QString m_socketPath;
    QLocalSocket m_socket;
    QByteArray m_buffer;
    quint64 m_nextRequestId {1};
    QString m_daemonVersion;
    int m_daemonProtocolVersion {0};
    QString m_lastError;
};

} // namespace lumacore
