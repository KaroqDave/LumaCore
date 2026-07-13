// SPDX-License-Identifier: GPL-2.0-or-later

#include "ipc/DaemonClient.h"

#include "ipc/DaemonFrameCodec.h"

#include <QEventLoop>

#include <utility>

namespace lumacore {

namespace {

constexpr int kInitialReconnectDelayMs = 250;
constexpr int kMaximumReconnectDelayMs = 5000;

int reconnectDelayMs(int attempt)
{
    const int exponent = qBound(0, attempt - 1, 5);
    return qMin(kInitialReconnectDelayMs * (1 << exponent), kMaximumReconnectDelayMs);
}

} // namespace

DaemonClient::DaemonClient(QString socketPath, QObject* parent)
    : QObject(parent)
    , m_socketPath(std::move(socketPath))
{
    m_socket.setReadBufferSize(kDaemonMaxFrameBytes);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (!m_automaticReconnectEnabled || m_manualDisconnect || isConnected()) {
            return;
        }
        startAsyncConnection();
    });
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        const bool recovered = m_reconnectAttempt > 0;
        resetReconnectState();
        m_manualDisconnect = false;
        setLastError(QString());
        emit connectionStateChanged();
        sendPendingCalls();
        if (recovered) {
            emit reconnected();
        }
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] {
        if (m_pendingCalls.isEmpty()) {
            const qint64 remainingCapacity = daemonFrameReadCapacity(m_buffer);
            if (remainingCapacity > 0 && m_socket.bytesAvailable() > 0) {
                m_buffer.append(m_socket.read(remainingCapacity));
            }
            return;
        }
        readAsyncResponses();
    });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        clearDaemonDryRunState();
        clearDaemonDiscoveryState();
        emit connectionStateChanged();
        if (!m_pendingCalls.isEmpty()) {
            failPendingCalls(
                m_lastError.isEmpty() ? QStringLiteral("LumaCore daemon disconnected.") : m_lastError
            );
        }
        scheduleReconnect();
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        setLastError(m_socket.errorString());
        if (m_socket.state() == QLocalSocket::UnconnectedState && !m_pendingCalls.isEmpty()) {
            failPendingCalls(m_lastError);
        }
        if (m_socket.state() == QLocalSocket::UnconnectedState) {
            scheduleReconnect();
        } else if (m_socket.state() == QLocalSocket::ConnectingState) {
            if (!m_pendingCalls.isEmpty()) {
                failPendingCalls(
                    m_lastError.isEmpty() ? QStringLiteral("Could not connect to LumaCore daemon.") : m_lastError
                );
            }
            m_socket.abort();
            scheduleReconnect();
        }
    });
}

DaemonClient::~DaemonClient()
{
    // Destroying a still-connected client would otherwise abort the socket
    // inside ~QLocalSocket, whose disconnected/errorOccurred handlers re-enter
    // members (the pending-call hash) that are destroyed before the socket.
    // Detach the handlers first, then tear the connection down while the
    // object is still intact.
    m_socket.disconnect(this);
    m_reconnectTimer.stop();
    m_socket.abort();
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
    if (m_automaticReconnectEnabled) {
        m_manualDisconnect = false;
        scheduleReconnect();
    }
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

bool DaemonClient::daemonDryRunKnown() const
{
    return m_daemonDryRunKnown;
}

bool DaemonClient::daemonDryRunEnabled() const
{
    return m_daemonDryRunEnabled;
}

bool DaemonClient::daemonDiscoveryKnown() const
{
    return m_daemonDiscoveryKnown;
}

bool DaemonClient::daemonDiscoveryComplete() const
{
    return m_daemonDiscoveryComplete;
}

bool DaemonClient::protocolCompatible() const
{
    // A value of 0 means the handshake has not reported a protocol version yet, so do not
    // raise a false mismatch before the daemon has been queried. This also avoids latching
    // "incompatible" permanently on the reconnect path, which never re-sends a status frame.
    return m_daemonProtocolVersion == 0 || m_daemonProtocolVersion == kDaemonProtocolVersion;
}

QString DaemonClient::lastError() const
{
    return m_lastError;
}

bool DaemonClient::automaticReconnectEnabled() const
{
    return m_automaticReconnectEnabled;
}

int DaemonClient::reconnectAttempt() const
{
    return m_reconnectAttempt;
}

bool DaemonClient::isConnecting() const
{
    return m_socket.state() == QLocalSocket::ConnectingState;
}

bool DaemonClient::isReconnectScheduled() const
{
    return m_reconnectTimer.isActive();
}

bool DaemonClient::connectToDaemon(int timeoutMs)
{
    m_manualDisconnect = false;
    return ensureConnected(timeoutMs);
}

void DaemonClient::disconnectFromDaemon()
{
    m_manualDisconnect = true;
    resetReconnectState();
    if (!m_pendingCalls.isEmpty()) {
        failPendingCalls(QStringLiteral("LumaCore daemon disconnected."));
    }
    m_buffer.clear();
    clearDaemonDryRunState();
    clearDaemonDiscoveryState();
    if (m_socket.state() == QLocalSocket::UnconnectedState) {
        emit connectionStateChanged();
        return;
    }

    m_socket.disconnectFromServer();
    if (m_socket.state() != QLocalSocket::UnconnectedState) {
        m_socket.waitForDisconnected(250);
    }
    emit connectionStateChanged();
}

void DaemonClient::setAutomaticReconnectEnabled(bool enabled)
{
    if (m_automaticReconnectEnabled == enabled) {
        return;
    }

    m_automaticReconnectEnabled = enabled;
    if (!enabled) {
        resetReconnectState();
        return;
    }

    m_manualDisconnect = false;
    if (!isConnected()) {
        reconnectNow();
    }
}

void DaemonClient::reconnectNow()
{
    m_manualDisconnect = false;
    m_reconnectTimer.stop();
    if (isConnected() || isConnecting()) {
        return;
    }

    // An immediate attempt does not consume a backoff slot. If it fails, the
    // first scheduled retry must still use the initial 250 ms delay.
    emit reconnectScheduled(qMax(1, m_reconnectAttempt), 0);
    m_reconnectTimer.start(0);
}

void DaemonClient::reportConnectionError(const QString& message)
{
    setLastError(message);
}

DaemonCallResult DaemonClient::call(const QString& method, const QJsonObject& params, int timeoutMs)
{
    DaemonCallResult result;
    bool finished = false;
    QEventLoop waitLoop;
    const quint64 requestId = callAsync(
        method,
        params,
        [&](DaemonCallResult response) {
            result = std::move(response);
            finished = true;
            waitLoop.quit();
        },
        timeoutMs
    );
    Q_UNUSED(requestId)
    if (!finished) {
        // Exclude user-input events while blocking so a click/keypress cannot
        // re-enter call() (or start another write) on top of this nested event
        // loop. Socket I/O and the timeout timer still run, so the response
        // still arrives and the loop terminates.
        waitLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    return result;
}

quint64 DaemonClient::callAsync(
    const QString& method,
    const QJsonObject& params,
    CallHandler handler,
    int timeoutMs
)
{
    const quint64 requestId = m_nextRequestId++;
    const QByteArray payload = encodeDaemonMessage(makeDaemonRequest(requestId, method, params));

    if (payload.size() > kDaemonMaxFrameBytes) {
        QTimer::singleShot(0, this, [this, handler = std::move(handler)]() mutable {
            const QString error = QStringLiteral("LumaCore daemon request exceeds the maximum message size.");
            setLastError(error);
            if (handler) {
                handler({false, {}, error});
            }
        });
        return requestId;
    }

    auto* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this, requestId] {
        finishPendingCall(
            requestId,
            {false, {}, QStringLiteral("Timed out waiting for LumaCore daemon response.")}
        );
    });

    m_pendingCalls.insert(requestId, PendingCall {
        .payload = payload,
        .handler = std::move(handler),
        .timeoutTimer = timeoutTimer,
    });
    m_pendingCallOrder.append(requestId);
    timeoutTimer->start(qMax(1, timeoutMs));

    // Defer transport work so callAsync() always returns before its handler can run,
    // including immediate connection and write failures.
    QTimer::singleShot(0, this, [this, requestId] {
        if (!m_pendingCalls.contains(requestId)) {
            return;
        }
        if (isConnected()) {
            sendPendingCall(requestId);
        } else {
            startAsyncConnection();
        }
    });
    return requestId;
}

bool DaemonClient::cancelCall(quint64 requestId)
{
    if (!m_pendingCalls.contains(requestId)) {
        return false;
    }

    finishPendingCall(
        requestId,
        {false, {}, QStringLiteral("LumaCore daemon request was cancelled.")}
    );
    return true;
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

void DaemonClient::setDaemonDryRunEnabled(bool enabled)
{
    if (m_daemonDryRunKnown && m_daemonDryRunEnabled == enabled) {
        return;
    }

    m_daemonDryRunKnown = true;
    m_daemonDryRunEnabled = enabled;
    emit daemonInfoChanged();
}

void DaemonClient::clearDaemonDryRunState()
{
    setDaemonScheduleSupported(false);
    if (!m_daemonDryRunKnown && !m_daemonDryRunEnabled) {
        return;
    }

    m_daemonDryRunKnown = false;
    m_daemonDryRunEnabled = false;
    emit daemonInfoChanged();
}

void DaemonClient::setDaemonDiscoveryComplete(bool complete)
{
    if (m_daemonDiscoveryKnown && m_daemonDiscoveryComplete == complete) {
        return;
    }

    m_daemonDiscoveryKnown = true;
    m_daemonDiscoveryComplete = complete;
    emit daemonInfoChanged();
}

void DaemonClient::clearDaemonDiscoveryState()
{
    if (!m_daemonDiscoveryKnown && m_daemonDiscoveryComplete) {
        return;
    }

    m_daemonDiscoveryKnown = false;
    m_daemonDiscoveryComplete = true;
    emit daemonInfoChanged();
}

bool DaemonClient::daemonScheduleSupported() const
{
    return m_daemonScheduleSupported;
}

void DaemonClient::setDaemonScheduleSupported(bool supported)
{
    if (m_daemonScheduleSupported == supported) {
        return;
    }

    m_daemonScheduleSupported = supported;
    emit daemonInfoChanged();
}

void DaemonClient::updateDaemonInfo(const DaemonCallResult& result)
{
    if (result.ok && result.result.contains(QStringLiteral("daemonVersion"))) {
        setDaemonVersion(result.result.value(QStringLiteral("daemonVersion")).toString());
        setDaemonDiscoveryComplete(
            result.result.value(QStringLiteral("discoveryComplete")).toBool(true)
        );
    }
    if (result.ok && result.result.contains(QStringLiteral("protocolVersion"))) {
        setDaemonProtocolVersion(result.result.value(QStringLiteral("protocolVersion")).toInt());
    }
    if (result.ok && result.result.contains(QStringLiteral("dryRunEnabled"))) {
        setDaemonDryRunEnabled(result.result.value(QStringLiteral("dryRunEnabled")).toBool(false));
    }
    if (result.ok && result.result.contains(QStringLiteral("scheduleSupported"))) {
        setDaemonScheduleSupported(result.result.value(QStringLiteral("scheduleSupported")).toBool(false));
    }
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

void DaemonClient::startAsyncConnection()
{
    if (m_socket.state() == QLocalSocket::ConnectedState) {
        sendPendingCalls();
        return;
    }
    if (m_socket.state() == QLocalSocket::ConnectingState) {
        return;
    }

    m_buffer.clear();
    m_manualDisconnect = false;
    m_socket.abort();
    m_socket.connectToServer(m_socketPath);
}

void DaemonClient::sendPendingCall(quint64 requestId)
{
    auto pending = m_pendingCalls.find(requestId);
    if (pending == m_pendingCalls.end() || pending->written || !isConnected()) {
        return;
    }

    if (m_socket.write(pending->payload) != pending->payload.size()) {
        const QString error =
            QStringLiteral("Could not write to LumaCore daemon: %1").arg(m_socket.errorString());
        setLastError(error);
        finishPendingCall(requestId, {false, {}, error});
        return;
    }
    pending->written = true;
}

void DaemonClient::sendPendingCalls()
{
    const QList<quint64> requestIds = m_pendingCallOrder;
    for (const quint64 requestId : requestIds) {
        sendPendingCall(requestId);
    }
}

void DaemonClient::readAsyncResponses()
{
    const qint64 remainingCapacity = daemonFrameReadCapacity(m_buffer);
    if (remainingCapacity > 0) {
        m_buffer.append(m_socket.read(remainingCapacity));
    }

    while (true) {
        const DaemonFrameResult frame = takeNextDaemonFrame(&m_buffer);
        if (frame.status == DaemonFrameStatus::NeedMoreData) {
            const qint64 remainingCapacity = daemonFrameReadCapacity(m_buffer);
            if (m_socket.bytesAvailable() > 0 && remainingCapacity > 0) {
                m_buffer.append(m_socket.read(remainingCapacity));
                continue;
            }
            return;
        }
        if (frame.status == DaemonFrameStatus::OversizedFrame) {
            const QString error = QStringLiteral("LumaCore daemon response exceeds the maximum message size.");
            setLastError(error);
            failPendingCalls(error);
            m_socket.abort();
            return;
        }
        if (frame.status == DaemonFrameStatus::EmptyFrame) {
            continue;
        }

        QJsonObject object;
        QString parseError;
        if (!parseDaemonFrameObject(frame.payload, &object, &parseError)) {
            const QString error =
                QStringLiteral("Invalid daemon response: %1").arg(parseError);
            setLastError(error);
            failPendingCalls(error);
            m_socket.abort();
            return;
        }

        const quint64 requestId = object.value(QStringLiteral("id")).toString().toULongLong();
        if (!m_pendingCalls.contains(requestId)) {
            qWarning("LumaCore daemon response for unknown request id %llu", requestId);
            continue;
        }

        DaemonCallResult result;
        if (object.value(QStringLiteral("ok")).toBool(false)) {
            result = {true, object.value(QStringLiteral("result")).toObject(), {}};
        } else {
            result = {
                false,
                {},
                object.value(QStringLiteral("error")).toString(QStringLiteral("Daemon request failed.")),
            };
        }
        finishPendingCall(requestId, std::move(result));
    }
}

void DaemonClient::finishPendingCall(quint64 requestId, DaemonCallResult result)
{
    auto pending = m_pendingCalls.find(requestId);
    if (pending == m_pendingCalls.end()) {
        return;
    }

    PendingCall completed = std::move(pending.value());
    m_pendingCalls.erase(pending);
    m_pendingCallOrder.removeAll(requestId);
    if (completed.timeoutTimer != nullptr) {
        completed.timeoutTimer->stop();
        completed.timeoutTimer->deleteLater();
    }

    updateDaemonInfo(result);
    if (result.ok) {
        setLastError(QString());
    } else {
        setLastError(result.error);
    }
    if (completed.handler) {
        completed.handler(std::move(result));
    }
}

void DaemonClient::failPendingCalls(const QString& error)
{
    const QList<quint64> requestIds = m_pendingCallOrder;
    for (const quint64 requestId : requestIds) {
        finishPendingCall(requestId, {false, {}, error});
    }
}

void DaemonClient::scheduleReconnect()
{
    if (!m_automaticReconnectEnabled || m_manualDisconnect || isConnected() || m_reconnectTimer.isActive()) {
        return;
    }

    ++m_reconnectAttempt;
    const int delayMs = reconnectDelayMs(m_reconnectAttempt);
    m_reconnectTimer.start(delayMs);
    emit reconnectScheduled(m_reconnectAttempt, delayMs);
}

void DaemonClient::resetReconnectState()
{
    m_reconnectTimer.stop();
    m_reconnectAttempt = 0;
}

} // namespace lumacore
