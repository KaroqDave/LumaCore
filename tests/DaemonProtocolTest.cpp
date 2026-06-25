#include "backends/daemon/DaemonRgbDevice.h"
#include "backends/daemon/DaemonBackend.h"
#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

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

    [[nodiscard]] QString discoveryIdentity() const override
    {
        return QStringLiteral("0B05:19AF");
    }

    [[nodiscard]] QString discoverySupportStage() const override
    {
        return QStringLiteral("guarded-write-backend");
    }

    [[nodiscard]] QString discoverySupportStatus() const override
    {
        return QStringLiteral("validated guarded write backend");
    }

    [[nodiscard]] QString discoverySupportFamily() const override
    {
        return QStringLiteral("ASUS Aura USB HID");
    }

    [[nodiscard]] QString discoverySupportNotes() const override
    {
        return QStringLiteral("Protocol metadata fixture.");
    }

    [[nodiscard]] bool discoveryCataloged() const override
    {
        return true;
    }

    [[nodiscard]] bool discoveryWriteCapableBackend() const override
    {
        return true;
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

class ZoneSupportSnapshotDevice final : public lumacore::RgbDevice
{
public:
    ZoneSupportSnapshotDevice()
        : RgbDevice(
              QStringLiteral("zone-support-device"),
              QStringLiteral("Zone Support Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Fixed Header"),
            lumacore::RgbZoneType::Motherboard,
            1
        ));
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Addressable Header"),
            lumacore::RgbZoneType::AddressableHeader,
            1
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        Q_UNUSED(zoneIndex)
        Q_UNUSED(color)
        return false;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneColorWrite
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported test capability.")};
    }

    [[nodiscard]] bool supportsEffect(int effectType) const override
    {
        return effectType == static_cast<int>(lumacore::RgbEffectType::Static)
            || effectType == static_cast<int>(lumacore::RgbEffectType::Rainbow);
    }

    [[nodiscard]] bool supportsZoneEffect(int zoneIndex, int effectType) const override
    {
        if (zoneIndex == 0) {
            return effectType == static_cast<int>(lumacore::RgbEffectType::Static);
        }
        if (zoneIndex == 1) {
            return supportsEffect(effectType);
        }
        return false;
    }

    [[nodiscard]] bool supportsZoneEffectSpeed(int zoneIndex, int effectType) const override
    {
        return zoneIndex == 1 && effectType == static_cast<int>(lumacore::RgbEffectType::Rainbow);
    }

    [[nodiscard]] bool supportsZoneEffectBrightness(int zoneIndex, int effectType) const override
    {
        return supportsZoneEffect(zoneIndex, effectType)
            && effectType == static_cast<int>(lumacore::RgbEffectType::Static);
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    using namespace lumacore;

    for (const DaemonMethod method : {
             DaemonMethod::Hello,
             DaemonMethod::Status,
             DaemonMethod::ListDevices,
             DaemonMethod::PreviewEffect,
             DaemonMethod::ApplyEffect,
             DaemonMethod::UpdateZone,
             DaemonMethod::ConfirmWrites,
             DaemonMethod::RevokeWrites,
             DaemonMethod::AllOff,
             DaemonMethod::SetDryRun,
             DaemonMethod::ActivityLogSnapshot,
         }) {
        if (!require(
                daemonMethodFromName(daemonMethodName(method)) == method,
                "daemon method names should round-trip without changing protocol strings"
            )) {
            return 1;
        }
    }
    if (!require(
            daemonMethodFromName(QStringLiteral("unknown")) == DaemonMethod::Unknown,
            "unknown daemon method names should remain distinguishable"
        )) {
        return 1;
    }

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

    DeviceManager exclusiveManager;
    DaemonServer firstExclusiveServer(&exclusiveManager);
    DaemonServer secondExclusiveServer(&exclusiveManager);
    const QString exclusiveServerName = testSocketName(QStringLiteral("exclusive"));
    QString firstExclusiveError;
    QString secondExclusiveError;
    if (!require(
            firstExclusiveServer.listen(exclusiveServerName, &firstExclusiveError),
            "first daemon server should acquire the endpoint"
        )
        || !require(
            !secondExclusiveServer.listen(exclusiveServerName, &secondExclusiveError),
            "second daemon server should be rejected"
        )
        || !require(
            secondExclusiveError.contains(QStringLiteral("already using endpoint")),
            "duplicate endpoint should report an actionable error"
        )) {
        return 1;
    }
    firstExclusiveServer.close();
    secondExclusiveError.clear();
    if (!require(
            secondExclusiveServer.listen(exclusiveServerName, &secondExclusiveError),
            "endpoint lock should be released when the first server closes"
        )) {
        return 1;
    }
    secondExclusiveServer.close();

#ifndef Q_OS_WIN
    DeviceManager caseSensitiveEndpointManager;
    DaemonServer lowerCaseEndpointServer(&caseSensitiveEndpointManager);
    DaemonServer upperCaseEndpointServer(&caseSensitiveEndpointManager);
    const QString lowerCaseEndpointName = testSocketName(QStringLiteral("case-sensitive"));
    const QString upperCaseEndpointName = lowerCaseEndpointName.toUpper();
    QString lowerCaseEndpointError;
    QString upperCaseEndpointError;
    if (!require(
            lowerCaseEndpointName != upperCaseEndpointName,
            "case-sensitive endpoint fixture should use distinct names"
        )
        || !require(
            lowerCaseEndpointServer.listen(lowerCaseEndpointName, &lowerCaseEndpointError),
            "lowercase Unix endpoint should listen"
        )
        || !require(
            upperCaseEndpointServer.listen(upperCaseEndpointName, &upperCaseEndpointError),
            "distinct uppercase Unix endpoint should listen"
        )) {
        return 1;
    }
    lowerCaseEndpointServer.close();
    upperCaseEndpointServer.close();
#endif

    DeviceManager dryRunGuardManager;
    std::vector<std::unique_ptr<RgbDevice>> dryRunGuardDevices;
    dryRunGuardDevices.push_back(std::make_unique<SnapshotDevice>(SnapshotDevice::Mode::ConfirmableAsus));
    dryRunGuardManager.replaceDevices(std::move(dryRunGuardDevices));
    dryRunGuardManager.setDryRunEnabled(true);
    DaemonServer dryRunGuardServer(&dryRunGuardManager);
    const QString dryRunGuardServerName = testSocketName(QStringLiteral("dryrun-guard"));
    QString dryRunGuardError;
    if (!require(
            dryRunGuardServer.listen(dryRunGuardServerName, &dryRunGuardError),
            "dry-run guard daemon server should listen"
        )) {
        return 1;
    }
    DaemonClient dryRunGuardClient(dryRunGuardServerName);
    const QJsonObject dryRunGuardEffect = RgbEffect(
        RgbEffectType::Static,
        RgbColor(1, 2, 3)
    ).toJson();
    const DaemonCallResult mismatchedApply = dryRunGuardClient.call(
        daemonMethodName(DaemonMethod::ApplyEffect),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("zoneIndex"), 0},
            {QStringLiteral("effect"), dryRunGuardEffect},
            {QStringLiteral("dryRunEnabled"), false},
        },
        3000
    );
    const DaemonCallResult matchedApply = dryRunGuardClient.call(
        daemonMethodName(DaemonMethod::ApplyEffect),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("zoneIndex"), 0},
            {QStringLiteral("effect"), dryRunGuardEffect},
            {QStringLiteral("dryRunEnabled"), true},
        },
        3000
    );
    const DaemonCallResult mismatchedAllOff = dryRunGuardClient.call(
        daemonMethodName(DaemonMethod::AllOff),
        {
            {QStringLiteral("deviceIndex"), 0},
            {QStringLiteral("dryRunEnabled"), false},
        },
        3000
    );
    if (!require(
            mismatchedApply.ok
                && !mismatchedApply.result.value(QStringLiteral("success")).toBool(true)
                && mismatchedApply.result.value(QStringLiteral("error")).toString().contains(QStringLiteral("dry-run mode")),
            "daemon should reject effect writes when dry-run state differs from the client"
        )
        || !require(
            matchedApply.ok && matchedApply.result.value(QStringLiteral("success")).toBool(false),
            "daemon should allow dry-run effect requests when dry-run state matches the client"
        )
        || !require(
            mismatchedAllOff.ok
                && !mismatchedAllOff.result.value(QStringLiteral("success")).toBool(true)
                && mismatchedAllOff.result.value(QStringLiteral("error")).toString().contains(QStringLiteral("dry-run mode")),
            "daemon should reject all-off writes when dry-run state differs from the client"
        )) {
        return 1;
    }
    dryRunGuardClient.disconnectFromDaemon();
    dryRunGuardServer.close();

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

    {
        const QString asyncServerName = testSocketName(QStringLiteral("async-correlation"));
        std::promise<bool> listeningPromise;
        std::future<bool> listeningFuture = listeningPromise.get_future();
        std::jthread asyncServerThread(
            [asyncServerName, promise = std::move(listeningPromise)]() mutable {
                QLocalServer::removeServer(asyncServerName);
                QLocalServer localServer;
                const bool listening = localServer.listen(asyncServerName);
                promise.set_value(listening);
                if (!listening || !localServer.waitForNewConnection(3000)) {
                    return;
                }

                QLocalSocket* socket = localServer.nextPendingConnection();
                if (socket == nullptr) {
                    return;
                }

                QList<QJsonObject> requests;
                QElapsedTimer requestTimer;
                requestTimer.start();
                while (requests.size() < 2 && requestTimer.elapsed() < 3000) {
                    if (!socket->canReadLine()) {
                        socket->waitForReadyRead(100);
                    }
                    while (socket->canReadLine()) {
                        requests.append(QJsonDocument::fromJson(socket->readLine()).object());
                    }
                }

                if (requests.size() == 2) {
                    for (int index = 1; index >= 0; --index) {
                        const QJsonObject& request = requests.at(index);
                        socket->write(encodeDaemonMessage(makeDaemonResult(
                            request.value(QStringLiteral("id")).toString().toULongLong(),
                            {{QStringLiteral("method"), request.value(QStringLiteral("method")).toString()}}
                        )));
                    }
                    socket->waitForBytesWritten(3000);
                }
                socket->disconnectFromServer();
                socket->waitForDisconnected(3000);
                delete socket;
            }
        );

        if (!require(listeningFuture.get(), "asynchronous daemon test server should listen")) {
            return 1;
        }

        DaemonClient asyncClient(asyncServerName);
        bool firstFinished = false;
        bool secondFinished = false;
        QString firstMethod;
        QString secondMethod;
        QElapsedTimer callTimer;
        callTimer.start();
        const quint64 firstRequestId = asyncClient.callAsync(
            QStringLiteral("first"),
            {},
            [&](DaemonCallResult result) {
                firstFinished = result.ok;
                firstMethod = result.result.value(QStringLiteral("method")).toString();
            },
            3000
        );
        const quint64 secondRequestId = asyncClient.callAsync(
            QStringLiteral("second"),
            {},
            [&](DaemonCallResult result) {
                secondFinished = result.ok;
                secondMethod = result.result.value(QStringLiteral("method")).toString();
            },
            3000
        );
        const qint64 callDurationMs = callTimer.elapsed();

        if (!require(firstRequestId != secondRequestId, "asynchronous requests should receive unique IDs")
            || !require(callDurationMs < 100, "asynchronous calls should return without waiting for daemon responses")
            || !require(
                waitUntil([&] { return firstFinished && secondFinished; }),
                "asynchronous requests should both complete"
            )
            || !require(
                firstMethod == QStringLiteral("first") && secondMethod == QStringLiteral("second"),
                "out-of-order daemon responses should reach their matching callbacks"
            )) {
            return 1;
        }

        asyncClient.disconnectFromDaemon();
        asyncServerThread.join();
    }

    {
        const QString timeoutServerName = testSocketName(QStringLiteral("async-timeout"));
        std::promise<bool> listeningPromise;
        std::future<bool> listeningFuture = listeningPromise.get_future();
        std::jthread timeoutServerThread(
            [timeoutServerName, promise = std::move(listeningPromise)]() mutable {
                QLocalServer::removeServer(timeoutServerName);
                QLocalServer localServer;
                const bool listening = localServer.listen(timeoutServerName);
                promise.set_value(listening);
                if (!listening || !localServer.waitForNewConnection(3000)) {
                    return;
                }

                QLocalSocket* socket = localServer.nextPendingConnection();
                if (socket != nullptr) {
                    socket->waitForReadyRead(3000);
                    QThread::msleep(150);
                    socket->disconnectFromServer();
                    delete socket;
                }
            }
        );

        if (!require(listeningFuture.get(), "asynchronous timeout test server should listen")) {
            return 1;
        }

        DaemonClient timeoutClient(timeoutServerName);
        bool timeoutFinished = false;
        QString timeoutError;
        const quint64 timeoutRequestId = timeoutClient.callAsync(
            QStringLiteral("noResponse"),
            {},
            [&](DaemonCallResult result) {
                timeoutFinished = true;
                timeoutError = result.error;
            },
            50
        );
        if (!require(timeoutRequestId > 0, "asynchronous timeout request should receive an ID")
            || !require(waitUntil([&] { return timeoutFinished; }), "asynchronous request timeout should complete")
            || !require(
                timeoutError.contains(QStringLiteral("Timed out")),
                "asynchronous timeout should report an actionable error"
            )) {
            return 1;
        }

        timeoutClient.disconnectFromDaemon();
        timeoutServerThread.join();
    }

    {
        DaemonClient cancellationClient(QStringLiteral("unused-cancellation-socket"));
        bool cancellationFinished = false;
        QString cancellationError;
        const quint64 cancellationRequestId = cancellationClient.callAsync(
            QStringLiteral("cancelled"),
            {},
            [&](DaemonCallResult result) {
                cancellationFinished = true;
                cancellationError = result.error;
            }
        );
        if (!require(
                cancellationClient.cancelCall(cancellationRequestId),
                "pending asynchronous requests should be cancellable"
            )
            || !require(cancellationFinished, "cancelling a request should complete its callback")
            || !require(
                cancellationError.contains(QStringLiteral("cancelled")),
                "cancelled requests should report their cancellation"
            )
            || !require(
                !cancellationClient.cancelCall(cancellationRequestId),
                "completed requests should not be cancellable twice"
            )) {
            return 1;
        }
    }

    {
        const QString reconnectServerName = testSocketName(QStringLiteral("automatic-reconnect"));
        QLocalServer reconnectServer;
        bool reconnectServerListened = false;
        QTimer::singleShot(100, [&] {
            QLocalServer::removeServer(reconnectServerName);
            reconnectServerListened = reconnectServer.listen(reconnectServerName);
        });

        DaemonClient reconnectClient(reconnectServerName);
        QList<int> reconnectDelays;
        bool reconnectSignalReceived = false;
        QObject::connect(
            &reconnectClient,
            &DaemonClient::reconnectScheduled,
            [&](int, int delayMs) { reconnectDelays.append(delayMs); }
        );
        QObject::connect(
            &reconnectClient,
            &DaemonClient::reconnected,
            [&] { reconnectSignalReceived = true; }
        );

        if (!require(
                !reconnectClient.connectToDaemon(50),
                "automatic reconnect fixture should begin disconnected"
            )) {
            return 1;
        }
        reconnectClient.setAutomaticReconnectEnabled(true);
        if (!require(
                waitUntil([&] { return reconnectSignalReceived && reconnectClient.isConnected(); }, 4000),
                "client should reconnect after the daemon becomes available"
            )
            || !require(reconnectServerListened, "automatic reconnect fixture server should listen")
            || !require(
                !reconnectDelays.isEmpty() && reconnectDelays.constFirst() == 250,
                "automatic reconnect should start with a short bounded delay"
            )
            || !require(
                std::all_of(
                    reconnectDelays.cbegin(),
                    reconnectDelays.cend(),
                    [](int delayMs) { return delayMs <= 5000; }
                ),
                "automatic reconnect delay should remain capped"
            )
            || !require(
                reconnectClient.reconnectAttempt() == 0,
                "successful reconnect should reset the attempt counter"
            )) {
            return 1;
        }

        const int schedulesBeforeDisconnect = reconnectDelays.size();
        reconnectClient.disconnectFromDaemon();
        QElapsedTimer intentionalDisconnectTimer;
        intentionalDisconnectTimer.start();
        while (intentionalDisconnectTimer.elapsed() < 400) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        if (!require(
                reconnectDelays.size() == schedulesBeforeDisconnect,
                "intentional disconnect should suppress automatic reconnect"
            )) {
            return 1;
        }
        reconnectServer.close();
    }

    {
        const QString deviceServerName = testSocketName(QStringLiteral("async-device"));
        std::promise<bool> listeningPromise;
        std::future<bool> listeningFuture = listeningPromise.get_future();
        bool asyncRequestIncludedDryRun = false;
        bool asyncRequestDryRunEnabled = true;
        std::jthread deviceServerThread(
            [
                deviceServerName,
                &asyncRequestIncludedDryRun,
                &asyncRequestDryRunEnabled,
                promise = std::move(listeningPromise)
            ]() mutable {
                QLocalServer::removeServer(deviceServerName);
                QLocalServer localServer;
                const bool listening = localServer.listen(deviceServerName);
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
                    const QJsonObject request = QJsonDocument::fromJson(socket->readLine()).object();
                    const QJsonObject params = request.value(QStringLiteral("params")).toObject();
                    asyncRequestIncludedDryRun = params.value(QStringLiteral("dryRunEnabled")).isBool();
                    asyncRequestDryRunEnabled = params.value(QStringLiteral("dryRunEnabled")).toBool(true);
                    QThread::msleep(100);
                    socket->write(encodeDaemonMessage(makeDaemonResult(
                        request.value(QStringLiteral("id")).toString().toULongLong(),
                        {
                            {QStringLiteral("success"), true},
                            {QStringLiteral("hardwareStatus"), QStringLiteral("Applied by async fixture.")},
                        }
                    )));
                    socket->waitForBytesWritten(3000);
                }
                socket->disconnectFromServer();
                socket->waitForDisconnected(3000);
                delete socket;
            }
        );

        if (!require(listeningFuture.get(), "asynchronous device test server should listen")) {
            return 1;
        }

        auto deviceClient = std::make_shared<DaemonClient>(deviceServerName);
        const QJsonObject deviceSnapshot {
            {QStringLiteral("index"), 0},
            {QStringLiteral("id"), QStringLiteral("async-device")},
            {QStringLiteral("name"), QStringLiteral("Async Device")},
            {QStringLiteral("vendor"), QStringLiteral("LumaCore")},
            {QStringLiteral("type"), QStringLiteral("Motherboard")},
            {QStringLiteral("backendId"), QStringLiteral("mock")},
            {QStringLiteral("capabilities"), QJsonArray {
                backendCapabilityToString(BackendCapability::DiscoveryRead),
                backendCapabilityToString(BackendCapability::ZoneColorWrite),
            }},
            {QStringLiteral("effectSupport"), QJsonArray {
                QJsonObject {
                    {QStringLiteral("effectType"), static_cast<int>(RgbEffectType::Static)},
                    {QStringLiteral("supported"), true},
                    {QStringLiteral("speed"), false},
                    {QStringLiteral("brightness"), true},
                },
            }},
            {QStringLiteral("zones"), QJsonArray {
                QJsonObject {
                    {QStringLiteral("name"), QStringLiteral("Zone 1")},
                    {QStringLiteral("type"), QStringLiteral("Motherboard")},
                    {QStringLiteral("ledCount"), 1},
                    {QStringLiteral("color"), QStringLiteral("#000000")},
                    {QStringLiteral("effect"), RgbEffect(
                        RgbEffectType::Static,
                        RgbColor(0, 0, 0)
                    ).toJson()},
                },
            }},
        };
        DaemonRgbDevice asyncDevice(deviceSnapshot, deviceClient);
        const RgbEffect requestedEffect(RgbEffectType::Static, RgbColor(17, 34, 51), 1.0, 75);
        bool operationFinished = false;
        bool operationSucceeded = false;
        QElapsedTimer operationTimer;
        operationTimer.start();
        const quint64 operationId = asyncDevice.applyZoneEffectAsync(
            0,
            requestedEffect,
            true,
            [&](bool success, const QString&) {
                operationFinished = true;
                operationSucceeded = success;
            }
        );
        const qint64 queueDurationMs = operationTimer.elapsed();

        if (!require(operationId > 0, "asynchronous device operation should receive a request ID")
            || !require(queueDurationMs < 100, "asynchronous device operation should not wait for hardware")
            || !require(
                asyncDevice.zoneEffect(0).color() == RgbColor(0, 0, 0),
                "proxy state should not change before daemon success"
            )
            || !require(waitUntil([&] { return operationFinished; }), "asynchronous device operation should complete")
            || !require(operationSucceeded, "asynchronous device operation should report success")
            || !require(
                asyncDevice.zoneEffect(0) == requestedEffect,
                "proxy state should update after daemon success"
            )
            || !require(
                asyncDevice.lastHardwareWriteStatus() == QStringLiteral("Applied by async fixture."),
                "asynchronous device operation should preserve hardware status"
            )) {
            return 1;
        }

        deviceClient->disconnectFromDaemon();
        deviceServerThread.join();
        if (!require(
                asyncRequestIncludedDryRun && !asyncRequestDryRunEnabled,
                "asynchronous proxy writes should include the GUI dry-run expectation"
            )) {
            return 1;
        }

        DaemonBackend snapshotBackend(deviceClient);
        DeviceManager refreshedManager;
        refreshedManager.replaceDevices(snapshotBackend.devicesFromPayload(QJsonObject {
            {QStringLiteral("backend"), backendDescriptorToJson(BackendDescriptor {
                QStringLiteral("mock"),
                QStringLiteral("Mock Backend"),
                QStringLiteral("Refresh fixture"),
                BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite,
            })},
            {QStringLiteral("devices"), QJsonArray {deviceSnapshot}},
        }));
        if (!require(
                refreshedManager.deviceCount() == 1
                    && refreshedManager.deviceAt(0)->id() == QStringLiteral("async-device"),
                "fresh daemon snapshots should atomically replace proxy devices"
            )
            || !require(
                snapshotBackend.descriptor().displayName == QStringLiteral("Daemon: Mock Backend"),
                "fresh daemon snapshots should update the backend descriptor"
            )) {
            return 1;
        }
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
    if (!require(
            unverifiedJson.value(QStringLiteral("discoveryIdentity")).toString() == QStringLiteral("0B05:19AF"),
            "daemon snapshots should serialize discovery identity"
        )
        || !require(
            unverifiedJson.value(QStringLiteral("discoverySupportStage")).toString() == QStringLiteral("guarded-write-backend"),
            "daemon snapshots should serialize discovery support stage"
        )
        || !require(
            unverifiedJson.value(QStringLiteral("discoveryCataloged")).toBool(false),
            "daemon snapshots should serialize catalog state"
        )
        || !require(
            unverifiedJson.value(QStringLiteral("discoveryWriteCapableBackend")).toBool(false),
            "daemon snapshots should serialize separate write-backend availability"
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

    const ZoneSupportSnapshotDevice zoneSupportDevice;
    const QJsonObject zoneSupportJson = deviceToJson(zoneSupportDevice, 2, false);
    auto zoneSupportClient = std::make_shared<DaemonClient>(QStringLiteral("unused-zone-support"));
    DaemonRgbDevice zoneSupportProxy(zoneSupportJson, zoneSupportClient);
    if (!require(
            zoneSupportProxy.supportsEffect(static_cast<int>(RgbEffectType::Rainbow)),
            "top-level daemon snapshots should still advertise device-wide effect support"
        )
        || !require(
            !zoneSupportProxy.supportsZoneEffect(0, static_cast<int>(RgbEffectType::Rainbow)),
            "daemon snapshots should preserve unsupported effects on fixed zones"
        )
        || !require(
            zoneSupportProxy.supportsZoneEffect(1, static_cast<int>(RgbEffectType::Rainbow)),
            "daemon snapshots should preserve supported effects on addressable zones"
        )
        || !require(
            zoneSupportProxy.supportsZoneEffectSpeed(1, static_cast<int>(RgbEffectType::Rainbow)),
            "daemon snapshots should preserve per-zone speed support"
        )
        || !require(
            !zoneSupportProxy.supportsZoneEffectBrightness(1, static_cast<int>(RgbEffectType::Rainbow)),
            "daemon snapshots should preserve per-zone brightness limits"
        )) {
        return 1;
    }

    auto supportClient = std::make_shared<DaemonClient>(QStringLiteral("unused-support"));
    DaemonRgbDevice supportProxy(unverifiedJson, supportClient);
    if (!require(
            supportProxy.discoveryIdentity() == QStringLiteral("0B05:19AF"),
            "daemon proxy should preserve discovery identity"
        )
        || !require(
            supportProxy.discoverySupportFamily() == QStringLiteral("ASUS Aura USB HID"),
            "daemon proxy should preserve discovery support family"
        )
        || !require(
            supportProxy.discoveryCataloged(),
            "daemon proxy should preserve discovery catalog state"
        )
        || !require(
            supportProxy.discoveryWriteCapableBackend(),
            "daemon proxy should preserve discovery write-backend marker"
        )) {
        return 1;
    }

    return 0;
}
