#include "ipc/DaemonClient.h"

#include <QJsonDocument>
#include <QJsonParseError>

#include <utility>

namespace lumacore {

DaemonClient::DaemonClient(QString socketPath, QObject* parent)
    : QObject(parent)
    , m_socketPath(std::move(socketPath))
{
    m_socket.setReadBufferSize(kDaemonMaxFrameBytes);
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        emit connectionStateChanged();
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        setLastError(m_socket.errorString());
    });
}

const QString& DaemonClient::socketPath() const
{
    return m_socketPath;
}

void DaemonClient::setSocketPath(QString socketPath)
{
    if (m_socketPath == socketPath) {
        return;
    }

    disconnectFromDaemon();
    m_socketPath = std::move(socketPath);
    emit daemonInfoChanged();
}

bool DaemonClient::isConnected() const
{
    return m_socket.state() == QLocalSocket::ConnectedState;
}

QString DaemonClient::daemonVersion() const
{
    return m_daemonVersion;
}

int DaemonClient::daemonProtocolVersion() const
{
    return m_daemonProtocolVersion;
}

bool DaemonClient::protocolCompatible() const
{
    // A value of 0 means the handshake has not reported a protocol version yet, so do not
    // raise a false mismatch before the daemon has been queried.
    return m_daemonProtocolVersion == 0 || m_daemonProtocolVersion == kDaemonProtocolVersion;
}

QString DaemonClient::lastError() const
{
    return m_lastError;
}

bool DaemonClient::connectToDaemon(int timeoutMs)
{
    return ensureConnected(timeoutMs);
}

void DaemonClient::disconnectFromDaemon()
{
    if (m_socket.state() == QLocalSocket::UnconnectedState) {
        return;
    }

    m_socket.disconnectFromServer();
    if (m_socket.state() != QLocalSocket::UnconnectedState) {
        m_socket.waitForDisconnected(250);
    }
    m_buffer.clear();
    emit connectionStateChanged();
}

void DaemonClient::reportConnectionError(const QString& message)
{
    setLastError(message);
}

DaemonCallResult DaemonClient::call(const QString& method, const QJsonObject& params, int timeoutMs)
{
    const quint64 requestId = m_nextRequestId++;
    const QByteArray payload = encodeDaemonMessage(makeDaemonRequest(requestId, method, params));
    if (payload.size() > kDaemonMaxFrameBytes) {
        setLastError(QStringLiteral("LumaCore daemon request exceeds the maximum message size."));
        return {false, {}, m_lastError};
    }
    if (!ensureConnected(timeoutMs)) {
        return {false, {}, m_lastError};
    }

    if (m_socket.write(payload) != payload.size() || !m_socket.waitForBytesWritten(timeoutMs)) {
        setLastError(QStringLiteral("Could not write to LumaCore daemon: %1").arg(m_socket.errorString()));
        disconnectFromDaemon();
        return {false, {}, m_lastError};
    }

    DaemonCallResult result = readResponse(requestId, timeoutMs);
    if (result.ok && result.result.contains(QStringLiteral("daemonVersion"))) {
        setDaemonVersion(result.result.value(QStringLiteral("daemonVersion")).toString());
    }
    if (result.ok && result.result.contains(QStringLiteral("protocolVersion"))) {
        setDaemonProtocolVersion(result.result.value(QStringLiteral("protocolVersion")).toInt());
    }
    if (!result.ok) {
        setLastError(result.error);
    }
    return result;
}

void DaemonClient::setLastError(const QString& message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
    emit daemonInfoChanged();
}

void DaemonClient::setDaemonVersion(const QString& version)
{
    if (m_daemonVersion == version) {
        return;
    }

    m_daemonVersion = version;
    emit daemonInfoChanged();
}

void DaemonClient::setDaemonProtocolVersion(int version)
{
    if (m_daemonProtocolVersion == version) {
        return;
    }

    m_daemonProtocolVersion = version;
    emit daemonInfoChanged();
}

bool DaemonClient::ensureConnected(int timeoutMs)
{
    if (isConnected()) {
        return true;
    }

    m_buffer.clear();
    m_socket.abort();
    m_socket.connectToServer(m_socketPath);
    if (!m_socket.waitForConnected(timeoutMs)) {
        setLastError(QStringLiteral("Could not connect to LumaCore daemon at %1: %2").arg(m_socketPath, m_socket.errorString()));
        emit connectionStateChanged();
        return false;
    }

    setLastError(QString());
    emit connectionStateChanged();
    return true;
}

DaemonCallResult DaemonClient::readResponse(quint64 requestId, int timeoutMs)
{
    while (true) {
        const qsizetype newlineIndex = m_buffer.indexOf('\n');
        if (newlineIndex >= 0) {
            if (newlineIndex > kDaemonMaxMessageBytes) {
                disconnectFromDaemon();
                return {false, {}, QStringLiteral("LumaCore daemon response exceeds the maximum message size.")};
            }

            const QByteArray line = m_buffer.left(newlineIndex);
            m_buffer.remove(0, newlineIndex + 1);
            if (line.trimmed().isEmpty()) {
                continue;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                return {false, {}, QStringLiteral("Invalid daemon response: %1").arg(parseError.errorString())};
            }

            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("id")).toString().toULongLong() != requestId) {
                continue;
            }

            if (!object.value(QStringLiteral("ok")).toBool(false)) {
                return {false, {}, object.value(QStringLiteral("error")).toString(QStringLiteral("Daemon request failed."))};
            }

            return {true, object.value(QStringLiteral("result")).toObject(), {}};
        }

        if (m_buffer.size() > kDaemonMaxMessageBytes) {
            disconnectFromDaemon();
            return {false, {}, QStringLiteral("LumaCore daemon response exceeds the maximum message size.")};
        }

        if (!m_socket.waitForReadyRead(timeoutMs)) {
            return {false, {}, QStringLiteral("Timed out waiting for LumaCore daemon response.")};
        }
        const qint64 remainingCapacity =
            static_cast<qint64>(kDaemonMaxFrameBytes) - m_buffer.size();
        m_buffer.append(m_socket.read(remainingCapacity));
    }
}

} // namespace lumacore
