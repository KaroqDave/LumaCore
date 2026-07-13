// SPDX-License-Identifier: GPL-2.0-or-later

#include "ipc/DaemonServer.h"

#include "app/Version.h"
#include "core/ProfileStore.h"
#include "core/ScheduleService.h"
#include "ipc/DaemonFrameCodec.h"
#include "ipc/DaemonProtocol.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QLockFile>
#include <QtGlobal>

#include <utility>

namespace {

QString endpointLockPath(const QString& socketPath)
{
    QString normalizedSocketPath = QDir::cleanPath(socketPath);
#ifdef Q_OS_WIN
    normalizedSocketPath = normalizedSocketPath.toLower();
#endif
    const QByteArray endpointHash = QCryptographicHash::hash(
        normalizedSocketPath.toUtf8(),
        QCryptographicHash::Sha256
    ).toHex();
    return QDir(QDir::tempPath()).filePath(
        QStringLiteral("lumacore-daemon-%1.lock").arg(QString::fromLatin1(endpointHash))
    );
}

bool dryRunMatchesClientExpectation(
    const QJsonObject& params,
    const lumacore::DeviceManager* deviceManager,
    QString* errorMessage
)
{
    if (deviceManager == nullptr) {
        return true;
    }

    // Write requests must declare an explicit dry-run expectation so the daemon
    // can refuse when its own state has drifted from the caller. A missing or
    // non-bool field is treated as a mismatch rather than silently allowed.
    const QJsonValue expectedDryRun = params.value(QStringLiteral("dryRunEnabled"));
    if (!expectedDryRun.isBool()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Daemon requires an explicit dry-run expectation on write requests; refusing hardware write."
            );
        }
        return false;
    }
    if (expectedDryRun.toBool() == deviceManager->dryRunEnabled()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral(
            "Daemon dry-run mode is not synchronized with the GUI; refusing hardware write."
        );
    }
    return false;
}

bool requiresCompletedDiscovery(lumacore::DaemonMethod method)
{
    switch (method) {
    case lumacore::DaemonMethod::PreviewEffect:
    case lumacore::DaemonMethod::ApplyEffect:
    case lumacore::DaemonMethod::UpdateZone:
    case lumacore::DaemonMethod::ConfirmWrites:
    case lumacore::DaemonMethod::RevokeWrites:
    case lumacore::DaemonMethod::AllOff:
    case lumacore::DaemonMethod::PaintZoneFrame:
        return true;
    case lumacore::DaemonMethod::Unknown:
    case lumacore::DaemonMethod::Hello:
    case lumacore::DaemonMethod::Status:
    case lumacore::DaemonMethod::ListDevices:
    case lumacore::DaemonMethod::SetDryRun:
    case lumacore::DaemonMethod::ActivityLogSnapshot:
    case lumacore::DaemonMethod::GetSchedule:
    case lumacore::DaemonMethod::SetSchedule:
    case lumacore::DaemonMethod::PutProfile:
        return false;
    }
    return false;
}

} // namespace

namespace lumacore {

DaemonServer::DaemonServer(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
{
    connect(&m_server, &QLocalServer::newConnection, this, &DaemonServer::handleNewConnection);
    if (m_deviceManager != nullptr) {
        connect(m_deviceManager, &DeviceManager::discoveryStateChanged, this, [this] {
            if (m_deviceManager->discoveryComplete()) {
                sendDeferredDeviceLists();
            }
        });
    }
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

    m_endpointLock = std::make_unique<QLockFile>(endpointLockPath(m_socketPath));
#ifdef Q_OS_WIN
    m_endpointLock->setStaleLockTime(30'000);
#endif
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

#ifndef Q_OS_WIN
    const QFileInfo socketInfo(m_socketPath);
    QDir socketDir(socketInfo.absolutePath());
    if (!socketDir.exists() && !socketDir.mkpath(QStringLiteral("."))) {
        const QString message = QStringLiteral("Could not create socket directory: %1").arg(socketDir.absolutePath());
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_endpointLock.reset();
        return false;
    }
#endif

    QLocalServer::removeServer(m_socketPath);
#ifdef Q_OS_WIN
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
#else
    // The daemon socket is the privilege boundary for hardware writes; Linux
    // packages grant GUI access by adding users to the configured service group.
    m_server.setSocketOptions(
        QLocalServer::UserAccessOption
        | QLocalServer::GroupAccessOption
    );
#endif
    if (!m_server.listen(m_socketPath)) {
        const QString message = QStringLiteral("Could not listen on %1: %2").arg(m_socketPath, m_server.errorString());
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_endpointLock.reset();
        m_ownsSocketEndpoint = false;
        return false;
    }

    m_ownsSocketEndpoint = true;
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
    m_deferredDeviceListRequestIds.clear();
    if (m_server.isListening()) {
        m_server.close();
    }
    if (m_ownsSocketEndpoint && !m_socketPath.isEmpty()) {
        QLocalServer::removeServer(m_socketPath);
    }
    m_ownsSocketEndpoint = false;
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

void DaemonServer::setScheduleService(ScheduleService* scheduleService)
{
    m_scheduleService = scheduleService;
}

void DaemonServer::handleNewConnection()
{
    while (QLocalSocket* socket = m_server.nextPendingConnection()) {
        QLocalSocket* previousClient =
            (m_activeClient != nullptr && m_activeClient != socket) ? m_activeClient : nullptr;

        // Register the replacement as the active client (and insert its buffer)
        // BEFORE tearing down the previous one. disconnectFromServer() can emit
        // disconnected() synchronously; if the new socket were not yet tracked,
        // handleDisconnected() would see an empty m_buffers and, under
        // --exit-on-disconnect, wrongly fire idleExitRequested() mid-handover.
        m_acceptedConnection = true;
        m_activeClient = socket;
        socket->setReadBufferSize(kDaemonMaxFrameBytes);
        m_buffers.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            handleReadyRead(socket);
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            handleDisconnected(socket);
        });

        if (previousClient != nullptr) {
            m_buffers.remove(previousClient);
            m_deferredDeviceListRequestIds.remove(previousClient);
            previousClient->disconnectFromServer();
            previousClient->deleteLater();
        }
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

        const DaemonFrameResult frame = takeNextDaemonFrame(&buffer);
        if (frame.status == DaemonFrameStatus::NeedMoreData) {
            const qint64 remainingCapacity = daemonFrameReadCapacity(buffer);
            if (socket->bytesAvailable() > 0 && remainingCapacity > 0) {
                buffer.append(socket->read(remainingCapacity));
                continue;
            }
            return;
        }

        if (frame.status == DaemonFrameStatus::OversizedFrame) {
            buffer.clear();
            socket->disconnectFromServer();
            return;
        }

        if (frame.status == DaemonFrameStatus::EmptyFrame) {
            continue;
        }

        QJsonObject request;
        QString parseError;
        const bool validRequest = parseDaemonFrameObject(frame.payload, &request, &parseError);
        const quint64 requestId = request.value(QStringLiteral("id")).toString().toULongLong();

        const bool deferDeviceList = validRequest
            && request.value(QStringLiteral("version")).toInt() == kDaemonProtocolVersion
            && daemonMethodFromName(request.value(QStringLiteral("method")).toString())
                == DaemonMethod::ListDevices
            && m_deviceManager != nullptr
            && !m_deviceManager->discoveryComplete()
            && !request.value(QStringLiteral("params"))
                    .toObject()
                    .value(QStringLiteral("acceptIncompleteDiscovery"))
                    .toBool(false);
        if (deferDeviceList) {
            m_deferredDeviceListRequestIds[socket].append(requestId);
            continue;
        }

        QJsonObject response;
        if (!validRequest) {
            response = makeDaemonError(requestId, QStringLiteral("Invalid JSON request: %1").arg(parseError));
        } else {
            response = handleRequest(request);
        }

        if (!writeResponse(socket, requestId, response)) {
            return;
        }
    }
}

bool DaemonServer::writeResponse(
    QLocalSocket* socket,
    quint64 requestId,
    const QJsonObject& response
)
{
    if (socket == nullptr || !m_buffers.contains(socket)) {
        return false;
    }

    QByteArray encodedResponse = encodeDaemonMessage(response);
    if (encodedResponse.size() > kDaemonMaxFrameBytes) {
        encodedResponse = encodeDaemonMessage(makeDaemonError(
            requestId,
            QStringLiteral("Daemon response exceeds the maximum message size.")
        ));
    }
    // flush() returning false is not an error: on Windows the overlapped
    // pipe writer often accepts the whole payload inside write(), leaving
    // nothing pending, and flush() reports false exactly in that case.
    // Only a short write is fatal for the connection.
    if (socket->write(encodedResponse) != encodedResponse.size()) {
        socket->disconnectFromServer();
        return false;
    }
    socket->flush();
    return true;
}

void DaemonServer::sendDeferredDeviceLists()
{
    const auto deferredRequests = std::exchange(
        m_deferredDeviceListRequestIds,
        QHash<QLocalSocket*, QList<quint64>> {}
    );
    for (auto socketIt = deferredRequests.constBegin(); socketIt != deferredRequests.constEnd(); ++socketIt) {
        QLocalSocket* socket = socketIt.key();
        for (const quint64 requestId : socketIt.value()) {
            if (!writeResponse(socket, requestId, makeDaemonResult(requestId, listDevicesPayload()))) {
                break;
            }
        }
    }
}

void DaemonServer::handleDisconnected(QLocalSocket* socket)
{
    m_buffers.remove(socket);
    m_deferredDeviceListRequestIds.remove(socket);
    if (socket == m_activeClient) {
        m_activeClient = nullptr;
    }
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
    const QString methodName = request.value(QStringLiteral("method")).toString();
    const DaemonMethod method = daemonMethodFromName(methodName);
    const QJsonObject params = request.value(QStringLiteral("params")).toObject();

    // Answer the hello/status handshake regardless of the request's protocol version so a
    // client running a different protocol can read protocolVersion and surface a clear
    // mismatch instead of every call failing opaquely.
    if (method == DaemonMethod::Hello || method == DaemonMethod::Status) {
        return makeDaemonResult(requestId, statusPayload());
    }

    if (request.value(QStringLiteral("version")).toInt() != kDaemonProtocolVersion) {
        return makeDaemonError(requestId, QStringLiteral("Unsupported daemon protocol version."));
    }
    if (m_deviceManager != nullptr
        && !m_deviceManager->discoveryComplete()
        && requiresCompletedDiscovery(method)) {
        return makeDaemonError(requestId, QStringLiteral("Device discovery is still in progress."));
    }
    switch (method) {
    case DaemonMethod::ListDevices:
        return makeDaemonResult(requestId, listDevicesPayload());
    case DaemonMethod::PreviewEffect:
        return makeDaemonResult(requestId, previewEffect(params));
    case DaemonMethod::ApplyEffect:
        return makeDaemonResult(requestId, applyEffect(params));
    case DaemonMethod::UpdateZone:
        return makeDaemonResult(requestId, updateZone(params));
    case DaemonMethod::ConfirmWrites:
        return makeDaemonResult(requestId, confirmWrites(params));
    case DaemonMethod::RevokeWrites:
        return makeDaemonResult(requestId, revokeWrites(params));
    case DaemonMethod::AllOff:
        return makeDaemonResult(requestId, allOff(params));
    case DaemonMethod::PaintZoneFrame:
        return makeDaemonResult(requestId, paintZoneFrame(params));
    case DaemonMethod::SetDryRun:
        return makeDaemonResult(requestId, setDryRun(params));
    case DaemonMethod::ActivityLogSnapshot:
        return makeDaemonResult(requestId, activityLogSnapshot());
    case DaemonMethod::GetSchedule:
        return makeDaemonResult(requestId, getSchedule());
    case DaemonMethod::SetSchedule:
        return makeDaemonResult(requestId, setSchedule(params));
    case DaemonMethod::PutProfile:
        return makeDaemonResult(requestId, putProfile(params));
    case DaemonMethod::Unknown:
    case DaemonMethod::Hello:
    case DaemonMethod::Status:
        break;
    }

    return makeDaemonError(requestId, QStringLiteral("Unknown daemon method: %1").arg(methodName));
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
        {QStringLiteral("discoveryComplete"), m_deviceManager == nullptr || m_deviceManager->discoveryComplete()},
        {QStringLiteral("scheduleSupported"), m_scheduleService != nullptr},
    };
}

QJsonObject DaemonServer::listDevicesPayload() const
{
    QJsonArray devices;
    if (m_deviceManager != nullptr) {
        for (int index = 0; index < m_deviceManager->deviceCount(); ++index) {
            const RgbDevice* device = m_deviceManager->deviceAt(index);
            if (device != nullptr) {
                devices.append(deviceToJson(
                    *device,
                    index,
                    m_deviceManager->deviceWriteConfirmed(index)
                ));
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
    QString dryRunError;
    if (!dryRunMatchesClientExpectation(params, m_deviceManager, &dryRunError)) {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("hardwareStatus"), dryRunError},
            {QStringLiteral("error"), dryRunError},
            {QStringLiteral("dryRunEnabled"), m_deviceManager->dryRunEnabled()},
        };
    }
    const bool dryRunEnabled = m_deviceManager->dryRunEnabled();
    const bool clientStreamsFrames = params.value(QStringLiteral("clientStreamsFrames")).toBool(false);
    const bool success = m_deviceManager->applyZoneEffect(deviceIndex, zoneIndex, effect, clientStreamsFrames);
    const QString hardwareStatus = success && dryRunEnabled
        ? QStringLiteral("Dry-run accepted; no hardware write was sent.")
        : (device == nullptr ? QString() : device->lastHardwareWriteStatus());
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("hardwareStatus"), hardwareStatus},
        {QStringLiteral("error"), success ? QString() : hardwareStatus},
        {QStringLiteral("dryRunEnabled"), dryRunEnabled},
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
    QString dryRunError;
    if (!dryRunMatchesClientExpectation(params, m_deviceManager, &dryRunError)) {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("error"), dryRunError},
            {QStringLiteral("hardwareStatus"), dryRunError},
            {QStringLiteral("dryRunEnabled"), m_deviceManager->dryRunEnabled()},
        };
    }
    const bool dryRunEnabled = m_deviceManager->dryRunEnabled();
    const bool success = m_deviceManager->applyAllOff(deviceIndex, &errorMessage);
    const QString hardwareStatus = success && dryRunEnabled
        ? QStringLiteral("Dry-run accepted; no hardware write was sent.")
        : (device == nullptr ? QString() : device->lastHardwareWriteStatus());
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("error"), errorMessage},
        {QStringLiteral("hardwareStatus"), hardwareStatus},
        {QStringLiteral("dryRunEnabled"), dryRunEnabled},
    };
}

QJsonObject DaemonServer::paintZoneFrame(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    const int deviceIndex = params.value(QStringLiteral("deviceIndex")).toInt(-1);
    const int zoneIndex = params.value(QStringLiteral("zoneIndex")).toInt(-1);
    QString dryRunError;
    if (!dryRunMatchesClientExpectation(params, m_deviceManager, &dryRunError)) {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("error"), dryRunError},
            {QStringLiteral("dryRunEnabled"), m_deviceManager->dryRunEnabled()},
        };
    }

    const bool dryRunEnabled = m_deviceManager->dryRunEnabled();
    const bool painted =
        m_deviceManager->paintZoneFrame(deviceIndex, zoneIndex, colorsFromJson(params.value(QStringLiteral("colors")).toArray()));
    // Under dry-run the frame is intentionally not written, which still counts as
    // an accepted call. Outside dry-run, report the real paint outcome so a
    // dropped frame (invalid target or write not permitted) is not masked.
    const bool success = dryRunEnabled || painted;
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("error"), success ? QString() : QStringLiteral("Frame was not applied (invalid target or write not permitted).")},
        {QStringLiteral("dryRunEnabled"), dryRunEnabled},
    };
}

QJsonObject DaemonServer::setDryRun(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("error"), QStringLiteral("Device manager is not available.")},
        };
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

QJsonObject DaemonServer::schedulePayload() const
{
    const ScheduleService::Config config = m_scheduleService->config();
    bool profileAvailable = false;
    if (m_deviceManager != nullptr && !config.profileName.isEmpty()) {
        const ProfileStore profileStore(m_deviceManager->profilesDirectoryPath());
        profileAvailable = profileStore.names().contains(ProfileStore::normalizeName(config.profileName));
    }
    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("enabled"), config.enabled},
        {QStringLiteral("profileName"), config.profileName},
        {QStringLiteral("time"), config.time},
        {QStringLiteral("profileAvailable"), profileAvailable},
    };
}

QJsonObject DaemonServer::getSchedule() const
{
    if (m_scheduleService == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Scheduling is not available.")}};
    }

    return schedulePayload();
}

QJsonObject DaemonServer::setSchedule(const QJsonObject& params)
{
    if (m_scheduleService == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Scheduling is not available.")}};
    }

    ScheduleService::Config config;
    config.enabled = params.value(QStringLiteral("enabled")).toBool(false);
    config.profileName = params.value(QStringLiteral("profileName")).toString();
    config.time = params.value(QStringLiteral("time")).toString();
    if (config.enabled && config.profileName.trimmed().isEmpty()) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Schedule requires a profile name.")}};
    }

    m_scheduleService->setConfig(config);
    return schedulePayload();
}

QJsonObject DaemonServer::putProfile(const QJsonObject& params)
{
    if (m_deviceManager == nullptr) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Device manager is not available.")}};
    }

    const QString profileName = params.value(QStringLiteral("profileName")).toString().trimmed();
    const QJsonValue profileValue = params.value(QStringLiteral("profile"));
    if (profileName.isEmpty() || !profileValue.isObject()) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Profile payload is missing or invalid.")}};
    }

    const QJsonObject profile = profileValue.toObject();
    // Legacy color-only profiles omit formatVersion; only a present-but-unknown
    // version is rejected so future formats cannot be silently misapplied.
    const QJsonValue formatVersion = profile.value(QStringLiteral("formatVersion"));
    if (!formatVersion.isUndefined() && formatVersion.toInt(-1) != 1) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), QStringLiteral("Unsupported profile format version.")}};
    }

    const QString normalizedName = ProfileStore::normalizeName(profileName);
    const ProfileStore profileStore(m_deviceManager->profilesDirectoryPath());

    QJsonObject existingProfile;
    if (profileStore.load(normalizedName, &existingProfile, nullptr) && existingProfile == profile) {
        return {{QStringLiteral("success"), true}, {QStringLiteral("profileName"), normalizedName}};
    }

    QString saveError;
    if (!profileStore.save(normalizedName, profile, &saveError)) {
        return {{QStringLiteral("success"), false}, {QStringLiteral("error"), saveError}};
    }

    m_deviceManager->activityLog().info(
        LogCategory::Profile,
        QStringLiteral("Stored profile '%1' for daemon scheduling.").arg(normalizedName)
    );
    return {{QStringLiteral("success"), true}, {QStringLiteral("profileName"), normalizedName}};
}

} // namespace lumacore
