#include "ipc/DaemonServer.h"

#include "app/Version.h"
#include "ipc/DaemonProtocol.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QtGlobal>

namespace {

#ifdef Q_OS_WIN
QString endpointLockPath(const QString& socketPath)
{
    const QByteArray endpointHash = QCryptographicHash::hash(
        socketPath.toLower().toUtf8(),
        QCryptographicHash::Sha256
    ).toHex();
    return QDir(QDir::tempPath()).filePath(
        QStringLiteral("lumacore-daemon-%1.lock").arg(QString::fromLatin1(endpointHash))
    );
}
#endif

} // namespace

namespace lumacore {

DaemonServer::DaemonServer(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
{
    connect(&m_server, &QLocalServer::newConnection, this, &DaemonServer::handleNewConnection);
}

DaemonServer::~DaemonServer()
{
    close();
}

bool DaemonServer::listen(const QString& socketPath, QString* errorMessage)
{
    close();
    m_socketPath = socketPath.isEmpty() ? defaultDaemonSocketPath() : socketPath;
    m_acceptedConnection = false;

#ifdef Q_OS_WIN
    m_endpointLock = std::make_unique<QLockFile>(endpointLockPath(m_socketPath));
    m_endpointLock->setStaleLockTime(0);
    if (!m_endpointLock->tryLock(0)) {
        const QString message = QStringLiteral(
            "Another LumaCore daemon is already using endpoint %1."
        ).arg(m_socketPath);
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_endpointLock.reset();
        return false;
    }
#else
    const QFileInfo socketInfo(m_socketPath);
    QDir socketDir(socketInfo.absolutePath());
    if (!socketDir.exists() && !socketDir.mkpath(QStringLiteral("."))) {
        const QString message = QStringLiteral("Could not create socket directory: %1").arg(socketDir.absolutePath());
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }
#endif

    QLocalServer::removeServer(m_socketPath);
#ifdef Q_OS_WIN
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
#else
    m_server.setSocketOptions(
        QLocalServer::UserAccessOption
        | QLocalServer::GroupAccessOption
        | QLocalServer::OtherAccessOption
    );
#endif
    if (!m_server.listen(m_socketPath)) {
        const QString message = QStringLiteral("Could not listen on %1: %2").arg(m_socketPath, m_server.errorString());
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_endpointLock.reset();
        return false;
    }

    return true;
}

void DaemonServer::close()
{
    m_closing = true;
    for (QLocalSocket* socket : m_buffers.keys()) {
        if (socket != nullptr) {
            socket->disconnectFromServer();
            socket->deleteLater();
        }
    }
    m_buffers.clear();
    if (m_server.isListening()) {
        m_server.close();
    }
    if (!m_socketPath.isEmpty()) {
        QLocalServer::removeServer(m_socketPath);
    }
    m_endpointLock.reset();
    m_closing = false;
}

QString DaemonServer::socketPath() const
{
    return m_socketPath;
}

void DaemonServer::setExitWhenIdle(bool enabled)
{
    m_exitWhenIdle = enabled;
}

void DaemonServer::handleNewConnection()
{
    while (QLocalSocket* socket = m_server.nextPendingConnection()) {
        m_acceptedConnection = true;
        socket->setReadBufferSize(kDaemonMaxFrameBytes);
        m_buffers.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            handleReadyRead(socket);
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            handleDisconnected(socket);
        });
    }
}

void DaemonServer::handleReadyRead(QLocalSocket* socket)
{
    if (socket == nullptr) {
        return;
    }

    while (true) {
        // Re-resolve the buffer every iteration: writing/flushing a response or
        // disconnecting can synchronously emit disconnected(), which runs
        // handleDisconnected() and erases this socket's entry, invalidating the
        // reference. Bail out as soon as the socket is no longer tracked.
        const auto bufferIt = m_buffers.find(socket);
        if (bufferIt == m_buffers.end()) {
            return;
        }
        QByteArray& buffer = bufferIt.value();

        const qsizetype newlineIndex = buffer.indexOf('\n');
        if (newlineIndex < 0) {
            if (buffer.size() > kDaemonMaxMessageBytes) {
                buffer.clear();
                socket->disconnectFromServer();
                return;
            }

            const qint64 remainingCapacity =
                static_cast<qint64>(kDaemonMaxFrameBytes) - buffer.size();
            if (socket->bytesAvailable() > 0 && remainingCapacity > 0) {
                buffer.append(socket->read(remainingCapacity));
                continue;
            }
            return;
        }

        if (newlineIndex > kDaemonMaxMessageBytes) {
            buffer.clear();
            socket->disconnectFromServer();
            return;
        }

        const QByteArray line = buffer.left(newlineIndex);
        buffer.remove(0, newlineIndex + 1);
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        const QJsonObject request = document.object();
        const quint64 requestId = request.value(QStringLiteral("id")).toString().toULongLong();

        QJsonObject response;
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            response = makeDaemonError(requestId, QStringLiteral("Invalid JSON request: %1").arg(parseError.errorString()));
        } else {
            response = handleRequest(request);
        }

        QByteArray encodedResponse = encodeDaemonMessage(response);
        if (encodedResponse.size() > kDaemonMaxFrameBytes) {
            encodedResponse =
                encodeDaemonMessage(makeDaemonError(requestId, QStringLiteral("Daemon response exceeds the maximum message size.")));
        }
        socket->write(encodedResponse);
        socket->flush();
    }
}

void DaemonServer::handleDisconnected(QLocalSocket* socket)
{
    m_buffers.remove(socket);
    if (socket != nullptr) {
        socket->deleteLater();
    }
    if (!m_closing && m_exitWhenIdle && m_acceptedConnection && m_buffers.isEmpty()) {
        emit idleExitRequested();
    }
}

QJsonObject DaemonServer::handleRequest(const QJsonObject& request)
{
    const quint64 requestId = request.value(QStringLiteral("id")).toString().toULongLong();
    const QString method = request.value(QStringLiteral("method")).toString();
    const QJsonObject params = request.value(QStringLiteral("params")).toObject();

    // Answer the hello/status handshake regardless of the request's protocol version so a
    // client running a different protocol can read protocolVersion and surface a clear
    // mismatch instead of every call failing opaquely.
    if (method == QStringLiteral("hello") || method == QStringLiteral("status")) {
        return makeDaemonResult(requestId, statusPayload());
    }

    if (request.value(QStringLiteral("version")).toInt() != kDaemonProtocolVersion) {
        return makeDaemonError(requestId, QStringLiteral("Unsupported daemon protocol version."));
    }
    if (method == QStringLiteral("listDevices")) {
        return makeDaemonResult(requestId, listDevicesPayload());
    }
    if (method == QStringLiteral("previewEffect")) {
        return makeDaemonResult(requestId, previewEffect(params));
    }
    if (method == QStringLiteral("applyEffect")) {
        return makeDaemonResult(requestId, applyEffect(params));
    }
    if (method == QStringLiteral("updateZone")) {
        return makeDaemonResult(requestId, updateZone(params));
    }
    if (method == QStringLiteral("confirmWrites")) {
        return makeDaemonResult(requestId, confirmWrites(params));
    }
    if (method == QStringLiteral("revokeWrites")) {
        return makeDaemonResult(requestId, revokeWrites(params));
    }
    if (method == QStringLiteral("allOff")) {
        return makeDaemonResult(requestId, allOff(params));
    }
    if (method == QStringLiteral("setDryRun")) {
        return makeDaemonResult(requestId, setDryRun(params));
    }
    if (method == QStringLiteral("activityLogSnapshot")) {
        return makeDaemonResult(requestId, activityLogSnapshot());
    }

    return makeDaemonError(requestId, QStringLiteral("Unknown daemon method: %1").arg(method));
}

QJsonObject DaemonServer::statusPayload() const
{
    const BackendDescriptor descriptor = m_deviceManager == nullptr ? BackendDescriptor {} : m_deviceManager->backendRegistry().activeDescriptor();
    return {
        {QStringLiteral("daemonVersion"), applicationVersion()},
        {QStringLiteral("protocolVersion"), kDaemonProtocolVersion},
        {QStringLiteral("socketPath"), m_socketPath},
        {QStringLiteral("backend"), backendDescriptorToJson(descriptor)},
        {QStringLiteral("deviceCount"), m_deviceManager == nullptr ? 0 : m_deviceManager->deviceCount()},
        {QStringLiteral("dryRunEnabled"), m_deviceManager != nullptr && m_deviceManager->dryRunEnabled()},
    };
}

QJsonObject DaemonServer::listDevicesPayload() const
{
    QJsonArray devices;
    if (m_deviceManager != nullptr) {
        for (int index = 0; index < m_deviceManager->deviceCount(); ++index) {
            const RgbDevice* device = m_deviceManager->deviceAt(index);
            if (device != nullptr) {
                devices.append(deviceToJson(*device, index, m_deviceManager->deviceWriteConfirmed(index)));
            }
        }
    }

    QJsonObject payload = statusPayload();
    payload.insert(QStringLiteral("devices"), devices);
    return payload;
}

QJsonObject DaemonServer::previewEffect(const QJsonObject& params) const
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    const int deviceIndex = params.value(QStringLiteral("deviceIndex")).toInt(-1);
    const int zoneIndex = params.value(QStringLiteral("zoneIndex")).toInt(-1);
    const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Invalid device or zone.")}};
    }

    const RgbEffect effect = effectFromJson(params.value(QStringLiteral("effect")).toObject());
    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("preview"), device->previewZoneEffectWrite(zoneIndex, effect)},
    };
}

QJsonObject DaemonServer::applyEffect(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    const int deviceIndex = params.value(QStringLiteral("deviceIndex")).toInt(-1);
    const int zoneIndex = params.value(QStringLiteral("zoneIndex")).toInt(-1);
    const RgbEffect effect = effectFromJson(params.value(QStringLiteral("effect")).toObject());
    const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
    const bool success = m_deviceManager->applyZoneEffect(deviceIndex, zoneIndex, effect);
    const QString hardwareStatus = device == nullptr ? QString() : device->lastHardwareWriteStatus();
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("hardwareStatus"), hardwareStatus},
        {QStringLiteral("error"), success ? QString() : hardwareStatus},
    };
}

QJsonObject DaemonServer::updateZone(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    QString errorMessage;
    const bool success = m_deviceManager->updateZone(
        params.value(QStringLiteral("deviceIndex")).toInt(-1),
        params.value(QStringLiteral("zoneIndex")).toInt(-1),
        params.value(QStringLiteral("name")).toString(),
        params.value(QStringLiteral("ledCount")).toInt(0),
        &errorMessage
    );
    return {{QStringLiteral("success"), success}, {QStringLiteral("error"), errorMessage}};
}

QJsonObject DaemonServer::confirmWrites(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    const int deviceIndex = params.value(QStringLiteral("deviceIndex")).toInt(-1);
    const bool success = m_deviceManager->confirmDeviceWrites(deviceIndex);
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("writeConfirmed"), success && m_deviceManager->deviceWriteConfirmed(deviceIndex)},
    };
}

QJsonObject DaemonServer::revokeWrites(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    const int deviceIndex = params.value(QStringLiteral("deviceIndex")).toInt(-1);
    const bool success = m_deviceManager->revokeDeviceWrites(deviceIndex);
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("writeConfirmed"), m_deviceManager->deviceWriteConfirmed(deviceIndex)},
    };
}

QJsonObject DaemonServer::allOff(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    QString errorMessage;
    const int deviceIndex = params.value(QStringLiteral("deviceIndex")).toInt(-1);
    const RgbDevice* device = m_deviceManager->deviceAt(deviceIndex);
    const bool success = m_deviceManager->applyAllOff(deviceIndex, &errorMessage);
    const QString hardwareStatus = device == nullptr ? QString() : device->lastHardwareWriteStatus();
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("error"), errorMessage},
        {QStringLiteral("hardwareStatus"), hardwareStatus},
    };
}

QJsonObject DaemonServer::setDryRun(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}};
    }

    m_deviceManager->setDryRunEnabled(params.value(QStringLiteral("enabled")).toBool(false));
    return {{QStringLiteral("success"), true}, {QStringLiteral("dryRunEnabled"), m_deviceManager->dryRunEnabled()}};
}

QJsonObject DaemonServer::activityLogSnapshot() const
{
    return {
        {QStringLiteral("lines"), QJsonArray::fromStringList(m_deviceManager == nullptr ? QStringList {} : m_deviceManager->activityLog().formattedLines())},
    };
}

} // namespace lumacore
