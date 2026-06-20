#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

#include <functional>
#include <future>
#include <thread>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

QString testSocketName(const QString& suffix)
{
    return QStringLiteral("lumacore-daemon-protocol-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(suffix);
}

struct ClientExchange {
    bool listening {false};
    bool connectedAfterCall {false};
    bool protocolCompatibleAfterCall {true};
    int daemonProtocolVersionAfterCall {0};
    QByteArray request;
    lumacore::DaemonCallResult result;
};

ClientExchange runClientExchange(
    const QString& suffix,
    const QString& method,
    const QJsonObject& params,
    const std::function<QByteArray(const QJsonObject&)>& responseBuilder
)
{
    const QString serverName = testSocketName(suffix);
    std::promise<bool> listeningPromise;
    std::future<bool> listeningFuture = listeningPromise.get_future();
    QByteArray receivedRequest;

    std::jthread serverThread(
        [serverName, &receivedRequest, responseBuilder, promise = std::move(listeningPromise)]() mutable {
            QLocalServer::removeServer(serverName);
            QLocalServer localServer;
            const bool listening = localServer.listen(serverName);
            promise.set_value(listening);
            if (!listening || !localServer.waitForNewConnection(3000)) {
                return;
            }

            QLocalSocket* socket = localServer.nextPendingConnection();
            if (socket == nullptr) {
                return;
            }

            while (!socket->canReadLine() && socket->waitForReadyRead(3000)) {
            }
            if (socket->canReadLine()) {
                receivedRequest = socket->readLine();
                const QJsonObject request = QJsonDocument::fromJson(receivedRequest).object();
                socket->write(responseBuilder(request));
                socket->waitForBytesWritten(3000);
                socket->waitForDisconnected(3000);
            }
            delete socket;
        }
    );

    ClientExchange exchange;
    exchange.listening = listeningFuture.get();
    if (!exchange.listening) {
        return exchange;
    }

    lumacore::DaemonClient client(serverName);
    exchange.result = client.call(method, params, 3000);
    exchange.connectedAfterCall = client.isConnected();
    exchange.protocolCompatibleAfterCall = client.protocolCompatible();
    exchange.daemonProtocolVersionAfterCall = client.daemonProtocolVersion();
    client.disconnectFromDaemon();
    serverThread.join();
    exchange.request = std::move(receivedRequest);
    return exchange;
}

class SnapshotDevice final : public lumacore::RgbDevice
{
public:
    enum class Mode {
        UnverifiedAsus,
        ConfirmableAsus,
    };

    explicit SnapshotDevice(Mode mode)
        : RgbDevice(
              mode == Mode::UnverifiedAsus ? QStringLiteral("asus-unverified") : QStringLiteral("asus-confirmable"),
              QStringLiteral("ASUS Aura HID Controller"),
              QStringLiteral("ASUS"),
              lumacore::RgbDeviceType::Motherboard
          )
        , m_mode(mode)
    {
        setBackendId(QStringLiteral("asus-aura-hid"));
        mutableZones().append(
            lumacore::RgbZone(
                QStringLiteral("Header 1"),
                lumacore::RgbZoneType::Motherboard,
                1,
                lumacore::RgbColor(0, 0, 0)
            )
        );
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        Q_UNUSED(zoneIndex)
        Q_UNUSED(color)
        return false;
    }

    [[nodiscard]] QString lastHardwareWriteStatus() const override
    {
        return m_mode == Mode::UnverifiedAsus
            ? QStringLiteral("ASUS Aura HID write skipped: config-table probe failed.")
            : QString();
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        if (m_mode == Mode::ConfirmableAsus) {
            return lumacore::BackendCapability::DiscoveryRead
                | lumacore::BackendCapability::ZoneColorWrite
                | lumacore::BackendCapability::ZoneEffectWrite;
        }
        return lumacore::BackendCapability::DiscoveryRead;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capability == lumacore::BackendCapability::DiscoveryRead) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        if (m_mode == Mode::ConfirmableAsus) {
            return {
                lumacore::PermissionStatus::RequiresConfirmation,
                QStringLiteral("ASUS Aura HID writes require confirmation before real hardware writes are allowed."),
            };
        }
        return {
            lumacore::PermissionStatus::Denied,
            QStringLiteral("ASUS Aura HID writes are disabled: ASUS Aura HID config-table probe failed for 0B05:19AF interface=2 path=/dev/hidraw4: timeout."),
        };
    }

private:
    Mode m_mode {Mode::UnverifiedAsus};
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    using namespace lumacore;

    DaemonClient client(QStringLiteral("unused-test-socket"));
    const DaemonCallResult oversizedRequest = client.call(
        QStringLiteral("test"),
        {{QStringLiteral("payload"), QString(kDaemonMaxMessageBytes, QLatin1Char('x'))}}
    );
    if (!require(
            !oversizedRequest.ok && oversizedRequest.error.contains(QStringLiteral("maximum message size")),
            "oversized daemon requests should be rejected before connecting"
        )) {
        return 1;
    }

    QJsonObject exactRequestParams {{QStringLiteral("payload"), QStringLiteral("x")}};
    const QByteArray shortRequest = encodeDaemonMessage(makeDaemonRequest(
        1,
        QStringLiteral("test"),
        exactRequestParams
    ));
    exactRequestParams.insert(
        QStringLiteral("payload"),
        QString(kDaemonMaxFrameBytes - shortRequest.size() + 1, QLatin1Char('x'))
    );
    if (!require(
            encodeDaemonMessage(makeDaemonRequest(1, QStringLiteral("test"), exactRequestParams)).size()
                == kDaemonMaxFrameBytes,
            "exact-limit client request fixture should include one delimiter byte"
        )) {
        return 1;
    }

    const ClientExchange exactRequestExchange = runClientExchange(
        QStringLiteral("exact-request"),
        QStringLiteral("test"),
        exactRequestParams,
        [](const QJsonObject& request) {
            return encodeDaemonMessage(makeDaemonResult(
                request.value(QStringLiteral("id")).toString().toULongLong()
            ));
        }
    );
    if (!require(
            exactRequestExchange.listening
                && exactRequestExchange.result.ok
                && exactRequestExchange.request.size() == kDaemonMaxFrameBytes,
            "client should send an exact-limit request"
        )) {
        return 1;
    }

    DeviceManager manager;
    DaemonServer server(&manager);
    const QString serverName = testSocketName(QStringLiteral("requests"));
    QString listenError;
    if (!require(server.listen(serverName, &listenError), "daemon test server should listen")) {
        return 1;
    }

    QLocalSocket oversizedSocket;
    oversizedSocket.connectToServer(serverName);
    if (!require(oversizedSocket.waitForConnected(1000), "oversized request client should connect")) {
        return 1;
    }
    QCoreApplication::processEvents();
    oversizedSocket.write(QByteArray(kDaemonMaxFrameBytes, 'x'));
    oversizedSocket.waitForBytesWritten(1000);
    if (!require(
            waitUntil([&oversizedSocket] { return oversizedSocket.state() == QLocalSocket::UnconnectedState; }),
            "daemon should disconnect oversized requests"
        )) {
        return 1;
    }
    if (!require(oversizedSocket.readAll().isEmpty(), "oversized requests should not receive an unmatched error response")) {
        return 1;
    }

    QLocalSocket boundarySocket;
    boundarySocket.connectToServer(serverName);
    if (!require(boundarySocket.waitForConnected(1000), "boundary request client should connect")) {
        return 1;
    }
    QCoreApplication::processEvents();
    const quint64 boundaryRequestId = 42;
    QByteArray boundaryMessage = encodeDaemonMessage(makeDaemonRequest(
        boundaryRequestId,
        QStringLiteral("x")
    ));
    const int methodLength = kDaemonMaxFrameBytes - boundaryMessage.size() + 1;
    const QString boundaryMethod(methodLength, QLatin1Char('x'));
    boundaryMessage = encodeDaemonMessage(makeDaemonRequest(boundaryRequestId, boundaryMethod));
    if (!require(
            boundaryMessage.size() == kDaemonMaxFrameBytes,
            "exact-limit daemon request should include one delimiter byte"
        )) {
        return 1;
    }
    boundarySocket.write(boundaryMessage);
    boundarySocket.waitForBytesWritten(1000);
    if (!require(
            waitUntil([&boundarySocket] { return boundarySocket.canReadLine(); }),
            "exact-limit daemon request should receive a response"
        )) {
        return 1;
    }
    const QJsonObject boundaryResponse = QJsonDocument::fromJson(boundarySocket.readLine()).object();
    if (!require(
            boundaryResponse.value(QStringLiteral("id")).toString().toULongLong() == boundaryRequestId
                && !boundaryResponse.value(QStringLiteral("ok")).toBool(true)
                && boundaryResponse.value(QStringLiteral("error")).toString().contains(QStringLiteral("maximum message size")),
            "oversized daemon responses should become matching protocol errors"
        )) {
        return 1;
    }
    boundarySocket.disconnectFromServer();
    server.close();

#ifdef Q_OS_WIN
    DeviceManager exclusiveManager;
    DaemonServer firstExclusiveServer(&exclusiveManager);
    DaemonServer secondExclusiveServer(&exclusiveManager);
    const QString exclusiveServerName = testSocketName(QStringLiteral("exclusive"));
    QString firstExclusiveError;
    QString secondExclusiveError;
    if (!require(
            firstExclusiveServer.listen(exclusiveServerName, &firstExclusiveError),
            "first Windows daemon server should acquire the endpoint"
        )
        || !require(
            !secondExclusiveServer.listen(exclusiveServerName, &secondExclusiveError),
            "second Windows daemon server should be rejected"
        )
        || !require(
            secondExclusiveError.contains(QStringLiteral("already using endpoint")),
            "duplicate Windows endpoint should report an actionable error"
        )) {
        return 1;
    }
    firstExclusiveServer.close();
    secondExclusiveError.clear();
    if (!require(
            secondExclusiveServer.listen(exclusiveServerName, &secondExclusiveError),
            "Windows endpoint lock should be released when the first server closes"
        )) {
        return 1;
    }
    secondExclusiveServer.close();
#endif

    const ClientExchange oversizedResponseExchange = runClientExchange(
        QStringLiteral("oversized-response"),
        QStringLiteral("status"),
        {},
        [](const QJsonObject&) {
            QByteArray response(kDaemonMaxFrameBytes, 'x');
            response.append('\n');
            return response;
        }
    );
    if (!require(
            oversizedResponseExchange.listening
                && !oversizedResponseExchange.result.ok
                && oversizedResponseExchange.result.error.contains(QStringLiteral("maximum message size"))
                && !oversizedResponseExchange.connectedAfterCall,
            "client should reject oversized responses and disconnect"
        )) {
        return 1;
    }

    QByteArray exactResponse = encodeDaemonMessage(makeDaemonResult(
        1,
        {{QStringLiteral("payload"), QStringLiteral("x")}}
    ));
    const int responsePayloadLength = kDaemonMaxFrameBytes - exactResponse.size() + 1;
    exactResponse = encodeDaemonMessage(makeDaemonResult(
        1,
        {{QStringLiteral("payload"), QString(responsePayloadLength, QLatin1Char('x'))}}
    ));
    if (!require(
            exactResponse.size() == kDaemonMaxFrameBytes,
            "exact-limit client response fixture should include one delimiter byte"
        )) {
        return 1;
    }

    const ClientExchange exactResponseExchange = runClientExchange(
        QStringLiteral("exact-response"),
        QStringLiteral("status"),
        {},
        [exactResponse](const QJsonObject&) { return exactResponse; }
    );
    if (!require(
            exactResponseExchange.listening
                && exactResponseExchange.result.ok
                && exactResponseExchange.connectedAfterCall,
            "client should accept an exact-limit response"
        )) {
        return 1;
    }

    const ClientExchange matchingProtocolExchange = runClientExchange(
        QStringLiteral("matching-protocol"),
        QStringLiteral("status"),
        {},
        [](const QJsonObject& request) {
            return encodeDaemonMessage(makeDaemonResult(
                request.value(QStringLiteral("id")).toString().toULongLong(),
                {{QStringLiteral("protocolVersion"), kDaemonProtocolVersion}}
            ));
        }
    );
    if (!require(
            matchingProtocolExchange.result.ok
                && matchingProtocolExchange.daemonProtocolVersionAfterCall == kDaemonProtocolVersion
                && matchingProtocolExchange.protocolCompatibleAfterCall,
            "client should accept a daemon advertising a matching protocol version"
        )) {
        return 1;
    }

    const ClientExchange mismatchedProtocolExchange = runClientExchange(
        QStringLiteral("mismatched-protocol"),
        QStringLiteral("status"),
        {},
        [](const QJsonObject& request) {
            return encodeDaemonMessage(makeDaemonResult(
                request.value(QStringLiteral("id")).toString().toULongLong(),
                {{QStringLiteral("protocolVersion"), kDaemonProtocolVersion + 1}}
            ));
        }
    );
    if (!require(
            mismatchedProtocolExchange.result.ok
                && mismatchedProtocolExchange.daemonProtocolVersionAfterCall == kDaemonProtocolVersion + 1
                && !mismatchedProtocolExchange.protocolCompatibleAfterCall,
            "client should flag a daemon advertising an incompatible protocol version"
        )) {
        return 1;
    }

    const SnapshotDevice unverified(SnapshotDevice::Mode::UnverifiedAsus);
    const QJsonObject unverifiedJson = deviceToJson(unverified, 0, false);
    const QJsonObject unverifiedPermissions = unverifiedJson.value(QStringLiteral("permissions")).toObject();
    const PermissionResult colorPermission = permissionResultFromJson(
        unverifiedPermissions.value(backendCapabilityToString(BackendCapability::ZoneColorWrite)).toObject()
    );
    if (!require(colorPermission.status == PermissionStatus::Denied, "unverified ASUS color permission should be denied")) {
        return 1;
    }
    if (!require(
            colorPermission.reason.contains(QStringLiteral("config-table probe failed")),
            "unverified ASUS permission reason should survive daemon JSON serialization"
        )) {
        return 1;
    }
    if (!require(unverifiedJson.value(QStringLiteral("readOnly")).toBool(false), "unverified ASUS snapshots should remain read-only")) {
        return 1;
    }
    if (!require(
            unverifiedJson.value(QStringLiteral("lastHardwareWriteStatus")).toString().contains(QStringLiteral("config-table probe failed")),
            "last hardware write status should be serialized"
        )) {
        return 1;
    }

    const SnapshotDevice confirmable(SnapshotDevice::Mode::ConfirmableAsus);
    const QJsonObject confirmedJson = deviceToJson(confirmable, 1, true);
    const PermissionResult confirmedPermission = permissionResultFromJson(
        confirmedJson.value(QStringLiteral("permission")).toObject()
    );
    if (!require(confirmedPermission.status == PermissionStatus::Granted, "confirmed daemon snapshots should upgrade write permission to granted")) {
        return 1;
    }
    if (!require(
            !confirmedJson.value(QStringLiteral("writeRequiresConfirmation")).toBool(true),
            "confirmed daemon snapshots should not still request confirmation"
        )) {
        return 1;
    }

    return 0;
}
