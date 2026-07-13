// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/daemon/DaemonRgbDevice.h"
#include "ipc/DaemonProtocol.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

#include <cstdio>
#include <functional>
#include <memory>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
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

QString endpoint(const QString& suffix)
{
    return QStringLiteral("lumacore-frame-coalescing-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(suffix);
}

class DelayedServer final
{
public:
    explicit DelayedServer(const QString& name)
        : m_name(name)
    {
        QLocalServer::removeServer(m_name);
        QObject::connect(&m_server, &QLocalServer::newConnection, [&] {
            m_socket = m_server.nextPendingConnection();
            if (m_socket == nullptr) {
                return;
            }
            QObject::connect(m_socket, &QLocalSocket::readyRead, [this] { readRequests(); });
            readRequests();
        });
    }

    ~DelayedServer()
    {
        m_server.close();
        QLocalServer::removeServer(m_name);
    }

    [[nodiscard]] bool listen()
    {
        return m_server.listen(m_name);
    }

    [[nodiscard]] int requestCount() const
    {
        return m_requests.size();
    }

    [[nodiscard]] const QJsonObject& request(int index) const
    {
        return m_requests.at(index);
    }

    void respondSuccess(int index)
    {
        if (m_socket == nullptr || m_socket->state() != QLocalSocket::ConnectedState) {
            return;
        }
        const quint64 requestId = m_requests.at(index)
                                      .value(QStringLiteral("id"))
                                      .toString()
                                      .toULongLong();
        m_socket->write(lumacore::encodeDaemonMessage(lumacore::makeDaemonResult(
            requestId,
            {{QStringLiteral("success"), true}}
        )));
        m_socket->flush();
    }

    void respondRejected(int index)
    {
        if (m_socket == nullptr || m_socket->state() != QLocalSocket::ConnectedState) {
            return;
        }
        const quint64 requestId = m_requests.at(index)
                                      .value(QStringLiteral("id"))
                                      .toString()
                                      .toULongLong();
        m_socket->write(lumacore::encodeDaemonMessage(lumacore::makeDaemonResult(
            requestId,
            {
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Frame rejected.")},
            }
        )));
        m_socket->flush();
    }

    void disconnectClient()
    {
        if (m_socket != nullptr) {
            m_socket->abort();
        }
    }

private:
    void readRequests()
    {
        if (m_socket == nullptr) {
            return;
        }
        m_buffer.append(m_socket->readAll());
        while (true) {
            const qsizetype newline = m_buffer.indexOf('\n');
            if (newline < 0) {
                return;
            }
            const QByteArray frame = m_buffer.left(newline);
            m_buffer.remove(0, newline + 1);
            if (!frame.isEmpty()) {
                m_requests.append(QJsonDocument::fromJson(frame).object());
            }
        }
    }

    QString m_name;
    QLocalServer m_server;
    QLocalSocket* m_socket {nullptr};
    QByteArray m_buffer;
    QVector<QJsonObject> m_requests;
};

QJsonObject deviceSnapshot()
{
    const QJsonArray effectSupport {
        QJsonObject {
            {QStringLiteral("effectType"), static_cast<int>(lumacore::RgbEffectType::Static)},
            {QStringLiteral("supported"), true},
            {QStringLiteral("speed"), false},
            {QStringLiteral("brightness"), true},
        },
        QJsonObject {
            {QStringLiteral("effectType"), static_cast<int>(lumacore::RgbEffectType::Strobe)},
            {QStringLiteral("supported"), true},
            {QStringLiteral("speed"), true},
            {QStringLiteral("brightness"), true},
        },
    };
    const auto zone = [&effectSupport](const QString& name) {
        return QJsonObject {
            {QStringLiteral("name"), name},
            {QStringLiteral("type"), QStringLiteral("AddressableHeader")},
            {QStringLiteral("ledCount"), 1},
            {QStringLiteral("color"), QStringLiteral("#000000")},
            {QStringLiteral("effect"), lumacore::RgbEffect(
                lumacore::RgbEffectType::Strobe,
                lumacore::RgbColor(255, 255, 255)
            ).toJson()},
            {QStringLiteral("effectSupport"), effectSupport},
        };
    };
    return {
        {QStringLiteral("index"), 0},
        {QStringLiteral("id"), QStringLiteral("coalescing-device")},
        {QStringLiteral("name"), QStringLiteral("Coalescing Device")},
        {QStringLiteral("vendor"), QStringLiteral("LumaCore")},
        {QStringLiteral("type"), QStringLiteral("Controller")},
        {QStringLiteral("backendId"), QStringLiteral("mock")},
        {QStringLiteral("capabilities"), QJsonArray {
            lumacore::backendCapabilityToString(lumacore::BackendCapability::DiscoveryRead),
            lumacore::backendCapabilityToString(lumacore::BackendCapability::ZoneColorWrite),
            lumacore::backendCapabilityToString(lumacore::BackendCapability::ZoneEffectWrite),
        }},
        {QStringLiteral("effectSupport"), effectSupport},
        {QStringLiteral("zones"), QJsonArray {
            zone(QStringLiteral("Zone 1")),
            zone(QStringLiteral("Zone 2")),
        }},
    };
}

QVector<lumacore::RgbColor> frame(int value)
{
    return {lumacore::RgbColor(value, 0, 0)};
}

int frameValue(const QJsonObject& request)
{
    const QJsonArray colors = request.value(QStringLiteral("params"))
                                  .toObject()
                                  .value(QStringLiteral("colors"))
                                  .toArray();
    const QVector<lumacore::RgbColor> decoded = lumacore::colorsFromJson(colors);
    return decoded.isEmpty() ? -1 : decoded.first().red();
}

QString method(const QJsonObject& request)
{
    return request.value(QStringLiteral("method")).toString();
}

bool testLatestFrameWins()
{
    DelayedServer server(endpoint(QStringLiteral("latest")));
    if (!require(server.listen(), "latest-frame server should listen")) {
        return false;
    }
    auto client = std::make_shared<lumacore::DaemonClient>(endpoint(QStringLiteral("latest")));
    lumacore::DaemonRgbDevice device(deviceSnapshot(), client);

    if (!require(device.applyZoneFrame(0, frame(10)), "first frame should be accepted")
        || !require(waitUntil([&] { return server.requestCount() == 1; }), "first frame should be sent")
        || !require(device.applyZoneFrame(0, frame(20)), "second frame should be coalesced")
        || !require(device.applyZoneFrame(0, frame(30)), "third frame should replace the second")
        || !require(device.applyZoneFrame(1, frame(40)), "another zone should progress independently")
        || !require(waitUntil([&] { return server.requestCount() == 2; }), "second zone should send concurrently")) {
        return false;
    }

    server.respondSuccess(0);
    if (!require(waitUntil([&] { return server.requestCount() == 3; }), "latest queued frame should drain")
        || !require(frameValue(server.request(0)) == 10, "first frame should remain first")
        || !require(frameValue(server.request(1)) == 40, "second zone should have its own in-flight request")
        || !require(frameValue(server.request(2)) == 30, "only the newest queued frame should be sent")
        || !require(
            device.zones().at(0).currentColor().red() == 30,
            "local preview should reflect the newest coalesced frame"
        )) {
        return false;
    }
    server.respondSuccess(1);
    server.respondSuccess(2);
    return true;
}

bool testControlOperationOrdering()
{
    DelayedServer server(endpoint(QStringLiteral("ordering")));
    if (!require(server.listen(), "ordering server should listen")) {
        return false;
    }
    auto client = std::make_shared<lumacore::DaemonClient>(endpoint(QStringLiteral("ordering")));
    lumacore::DaemonRgbDevice device(deviceSnapshot(), client);

    if (!require(device.applyZoneFrame(0, frame(50)), "ordering frame should be accepted")
        || !require(waitUntil([&] { return server.requestCount() == 1; }), "ordering frame should be sent")
        || !require(device.applyZoneFrame(0, frame(51)), "stale ordering frame should queue")) {
        return false;
    }

    bool effectFinished = false;
    const quint64 effectRequestId = device.applyZoneEffectAsync(
        0,
        lumacore::RgbEffect(lumacore::RgbEffectType::Static, lumacore::RgbColor(1, 2, 3)),
        false,
        [&](bool, const QString&) { effectFinished = true; }
    );
    if (!require(effectRequestId != 0, "effect request should be queued")
        || !require(waitUntil([&] { return server.requestCount() == 2; }), "effect request should be sent")
        || !require(!device.applyZoneFrame(0, frame(52)), "frames should be rejected during effect apply")) {
        return false;
    }
    server.respondSuccess(0);
    server.respondSuccess(1);
    if (!require(waitUntil([&] { return effectFinished; }), "effect request should complete")) {
        return false;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    if (!require(server.requestCount() == 2, "superseded effect frames should not drain")
        || !require(device.applyZoneFrame(0, frame(53)), "frames should resume after effect apply")
        || !require(waitUntil([&] { return server.requestCount() == 3; }), "resumed frame should be sent")) {
        return false;
    }
    server.respondSuccess(2);

    bool metadataFinished = false;
    const quint64 metadataRequestId = device.updateZoneMetadataAsync(
        0,
        QStringLiteral("Updated"),
        2,
        [&](bool, const QString&) { metadataFinished = true; }
    );
    if (!require(metadataRequestId != 0, "metadata request should be queued")
        || !require(waitUntil([&] { return server.requestCount() == 4; }), "metadata request should be sent")
        || !require(!device.applyZoneFrame(0, frame(54)), "frames should be rejected during metadata update")) {
        return false;
    }
    server.respondSuccess(3);
    if (!require(waitUntil([&] { return metadataFinished; }), "metadata request should complete")) {
        return false;
    }

    bool allOffFinished = false;
    const quint64 allOffRequestId = device.applyAllOffAsync(
        false,
        [&](bool, const QString&) { allOffFinished = true; }
    );
    if (!require(allOffRequestId != 0, "all-off request should be queued")
        || !require(waitUntil([&] { return server.requestCount() == 5; }), "all-off request should be sent")
        || !require(!device.applyZoneFrame(0, frame(55)), "zone one should suspend during all-off")
        || !require(!device.applyZoneFrame(1, frame(56)), "zone two should suspend during all-off")) {
        return false;
    }
    server.respondSuccess(4);
    return require(waitUntil([&] { return allOffFinished; }), "all-off request should complete")
        && require(server.requestCount() == 5, "control operations should not release stale frames");
}

bool testFailureDropsQueuedFrame(const QString& suffix, bool disconnect)
{
    const QString name = endpoint(suffix);
    DelayedServer server(name);
    if (!require(server.listen(), "failure server should listen")) {
        return false;
    }
    auto client = std::make_shared<lumacore::DaemonClient>(name);
    lumacore::DaemonRgbDevice device(deviceSnapshot(), client);
    if (!require(device.applyZoneFrame(0, frame(60)), "failure frame should be accepted")
        || !require(waitUntil([&] { return server.requestCount() == 1; }), "failure frame should be sent")
        || !require(device.applyZoneFrame(0, frame(61)), "failure frame should queue")) {
        return false;
    }

    if (disconnect) {
        server.disconnectClient();
        if (!require(waitUntil([&] { return !client->isConnected(); }), "client should observe disconnect")) {
            return false;
        }
    } else {
        QElapsedTimer timeout;
        timeout.start();
        while (timeout.elapsed() < 650) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return require(server.requestCount() == 1, "failed in-flight requests should drop queued frames");
}

bool testApplicationFailureDropsQueuedFrame()
{
    DelayedServer server(endpoint(QStringLiteral("rejected")));
    if (!require(server.listen(), "application-failure server should listen")) {
        return false;
    }
    auto client = std::make_shared<lumacore::DaemonClient>(endpoint(QStringLiteral("rejected")));
    lumacore::DaemonRgbDevice device(deviceSnapshot(), client);
    if (!require(device.applyZoneFrame(0, frame(70)), "application-failure frame should be accepted")
        || !require(waitUntil([&] { return server.requestCount() == 1; }), "application-failure frame should be sent")
        || !require(device.applyZoneFrame(0, frame(71)), "application-failure frame should queue")) {
        return false;
    }

    server.respondRejected(0);
    QElapsedTimer responseWait;
    responseWait.start();
    while (responseWait.elapsed() < 75) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return require(
        server.requestCount() == 1,
        "application-level frame rejection should drop the queued frame"
    );
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    return testLatestFrameWins()
            && testControlOperationOrdering()
            && testApplicationFailureDropsQueuedFrame()
            && testFailureDropsQueuedFrame(QStringLiteral("disconnect"), true)
            && testFailureDropsQueuedFrame(QStringLiteral("timeout"), false)
        ? 0
        : 1;
}
