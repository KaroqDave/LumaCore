// SPDX-License-Identifier: GPL-2.0-or-later

// Adversarial-input coverage for the real DaemonServer read loop. Where
// DaemonProtocolTest already pins the oversized-frame disconnect and every
// client-side path (reconnect, cancellation, timeout, out-of-order
// correlation), this test drives the server directly with a raw QLocalSocket to
// exercise the request-handling failure paths a well-behaved DaemonClient never
// produces: unknown methods, syntactically valid framing wrapping invalid JSON,
// protocol-version mismatches, empty frames interleaved with real requests,
// multiple pipelined requests drained from one buffer, a request split across
// writes, and a client that disconnects mid-frame. Each case asserts the server
// answers on the same connection where it should and cleans up without wedging
// where it should.

#include "core/DeviceManager.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QString>
#include <QThread>

#include <functional>

namespace {

using namespace lumacore;

int g_failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << "FAIL:" << message;
        ++g_failures;
    }
}

// Pump the event loop until the condition holds or the timeout expires. The
// server and client share this thread, so the server only makes progress while
// we spin here.
bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 2000)
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
    return QStringLiteral("lumacore-server-failure-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(suffix);
}

// A thin raw-socket client: it sends exactly the bytes it is handed (well-formed
// or not) and reads newline-delimited response frames, deliberately bypassing
// DaemonClient's framing so malformed input reaches the server verbatim.
class RawClient {
public:
    [[nodiscard]] bool connect(const QString& serverName)
    {
        m_socket.connectToServer(serverName);
        const bool connected = m_socket.waitForConnected(1000);
        QCoreApplication::processEvents();
        return connected;
    }

    void sendRaw(const QByteArray& bytes)
    {
        m_socket.write(bytes);
        m_socket.waitForBytesWritten(1000);
    }

    void sendRequest(quint64 id, const QString& method, const QJsonObject& params = {})
    {
        sendRaw(encodeDaemonMessage(makeDaemonRequest(id, method, params)));
    }

    // Read the next response object, pumping events so the server can produce it.
    [[nodiscard]] QJsonObject readResponse(int timeoutMs = 2000)
    {
        if (!waitUntil([this] { return m_socket.canReadLine(); }, timeoutMs)) {
            return {};
        }
        return QJsonDocument::fromJson(m_socket.readLine()).object();
    }

    [[nodiscard]] bool hasBufferedLine() const { return m_socket.canReadLine(); }

    void disconnect()
    {
        m_socket.disconnectFromServer();
        if (m_socket.state() != QLocalSocket::UnconnectedState) {
            m_socket.waitForDisconnected(1000);
        }
        QCoreApplication::processEvents();
    }

private:
    QLocalSocket m_socket;
};

// A server bound to a unique endpoint for one case, torn down on scope exit.
class ServerFixture {
public:
    explicit ServerFixture(const QString& suffix)
        : m_server(&m_manager)
        , m_name(testSocketName(suffix))
    {
        QString error;
        m_listening = m_server.listen(m_name, &error);
        if (!m_listening) {
            qCritical().noquote() << "FAIL: fixture server should listen:" << error;
            ++g_failures;
        }
    }

    ~ServerFixture() { m_server.close(); }

    [[nodiscard]] const QString& name() const { return m_name; }
    [[nodiscard]] bool listening() const { return m_listening; }

private:
    DeviceManager m_manager;
    DaemonServer m_server;
    QString m_name;
    bool m_listening {false};
};

QString responseId(const QJsonObject& response)
{
    return response.value(QStringLiteral("id")).toString();
}

bool responseIsError(const QJsonObject& response)
{
    return !response.value(QStringLiteral("ok")).toBool(true);
}

QString responseError(const QJsonObject& response)
{
    return response.value(QStringLiteral("error")).toString();
}

void testUnknownMethod()
{
    ServerFixture fixture(QStringLiteral("unknown-method"));
    RawClient client;
    check(client.connect(fixture.name()), "unknown-method client should connect");

    client.sendRequest(7, QStringLiteral("definitelyNotARealMethod"));
    const QJsonObject response = client.readResponse();
    check(responseId(response) == QStringLiteral("7"), "unknown-method error must echo the request id");
    check(responseIsError(response), "an unknown method must produce an error response");
    check(
        responseError(response).contains(QStringLiteral("Unknown daemon method")),
        "the error must name the unknown-method failure"
    );
    client.disconnect();
}

void testMalformedJson()
{
    ServerFixture fixture(QStringLiteral("malformed-json"));
    RawClient client;
    check(client.connect(fixture.name()), "malformed-json client should connect");

    // Valid framing (single trailing newline) around syntactically broken JSON.
    client.sendRaw(QByteArrayLiteral("{ this is not valid json }\n"));
    const QJsonObject response = client.readResponse();
    check(responseIsError(response), "malformed JSON must produce an error response, not a silent drop");
    check(
        responseError(response).contains(QStringLiteral("Invalid JSON request")),
        "the error must report an invalid JSON request"
    );
    check(
        responseId(response) == QStringLiteral("0"),
        "an unparseable request has no readable id, so the error id defaults to 0"
    );
    client.disconnect();
}

void testConnectionSurvivesMalformedRequest()
{
    // A bad frame must not poison the connection: a valid request that follows on
    // the same socket must still be answered.
    ServerFixture fixture(QStringLiteral("survive-malformed"));
    RawClient client;
    check(client.connect(fixture.name()), "survive-malformed client should connect");

    client.sendRaw(QByteArrayLiteral("{bad\n"));
    const QJsonObject errorResponse = client.readResponse();
    check(responseIsError(errorResponse), "the malformed frame should be rejected first");

    client.sendRequest(11, daemonMethodName(DaemonMethod::Status));
    const QJsonObject statusResponse = client.readResponse();
    check(
        responseId(statusResponse) == QStringLiteral("11")
            && statusResponse.value(QStringLiteral("ok")).toBool(false),
        "a valid request after a malformed one must still be answered on the same connection"
    );
    client.disconnect();
}

void testVersionMismatchOnRealMethod()
{
    // A non-handshake method carrying a wrong protocol version is rejected before
    // dispatch.
    ServerFixture fixture(QStringLiteral("version-mismatch"));
    RawClient client;
    check(client.connect(fixture.name()), "version-mismatch client should connect");

    QJsonObject request = makeDaemonRequest(3, daemonMethodName(DaemonMethod::ListDevices));
    request.insert(QStringLiteral("version"), kDaemonProtocolVersion + 1);
    client.sendRaw(encodeDaemonMessage(request));

    const QJsonObject response = client.readResponse();
    check(responseId(response) == QStringLiteral("3"), "version-mismatch error must echo the request id");
    check(
        responseIsError(response)
            && responseError(response).contains(QStringLiteral("Unsupported daemon protocol version")),
        "a real method on a mismatched protocol version must be refused"
    );
    client.disconnect();
}

void testHandshakeIgnoresVersion()
{
    // Hello/Status must answer regardless of protocol version so a mismatched
    // client can read protocolVersion and surface a clear error instead of an
    // opaque failure on every call.
    ServerFixture fixture(QStringLiteral("handshake-any-version"));
    RawClient client;
    check(client.connect(fixture.name()), "handshake-any-version client should connect");

    QJsonObject request = makeDaemonRequest(5, daemonMethodName(DaemonMethod::Status));
    request.insert(QStringLiteral("version"), kDaemonProtocolVersion + 99);
    client.sendRaw(encodeDaemonMessage(request));

    const QJsonObject response = client.readResponse();
    check(
        responseId(response) == QStringLiteral("5")
            && response.value(QStringLiteral("ok")).toBool(false),
        "status must succeed even when the request's protocol version differs"
    );
    check(
        response.value(QStringLiteral("result")).toObject()
                .value(QStringLiteral("protocolVersion")).toInt()
            == kDaemonProtocolVersion,
        "the handshake must still advertise the daemon's own protocol version"
    );
    client.disconnect();
}

void testEmptyFrameIsSkipped()
{
    // A leading blank line (empty frame) must be skipped without disturbing the
    // request that follows it in the same write.
    ServerFixture fixture(QStringLiteral("empty-frame"));
    RawClient client;
    check(client.connect(fixture.name()), "empty-frame client should connect");

    QByteArray payload = QByteArrayLiteral("\n");
    payload.append(encodeDaemonMessage(makeDaemonRequest(9, daemonMethodName(DaemonMethod::Status))));
    client.sendRaw(payload);

    const QJsonObject response = client.readResponse();
    check(
        responseId(response) == QStringLiteral("9")
            && response.value(QStringLiteral("ok")).toBool(false),
        "an empty frame must be skipped and the following request answered"
    );
    client.disconnect();
}

void testPipelinedRequestsDrainInOrder()
{
    // Two requests written back-to-back in a single buffer must both be drained
    // by the server read loop and answered in arrival order.
    ServerFixture fixture(QStringLiteral("pipelined"));
    RawClient client;
    check(client.connect(fixture.name()), "pipelined client should connect");

    QByteArray payload;
    payload.append(encodeDaemonMessage(makeDaemonRequest(101, daemonMethodName(DaemonMethod::Status))));
    payload.append(encodeDaemonMessage(makeDaemonRequest(102, daemonMethodName(DaemonMethod::ListDevices))));
    client.sendRaw(payload);

    const QJsonObject first = client.readResponse();
    const QJsonObject second = client.readResponse();
    check(responseId(first) == QStringLiteral("101"), "the first pipelined response must match the first request id");
    check(responseId(second) == QStringLiteral("102"), "the second pipelined response must match the second request id");
    check(
        first.value(QStringLiteral("ok")).toBool(false) && second.value(QStringLiteral("ok")).toBool(false),
        "both pipelined requests must succeed"
    );
    client.disconnect();
}

void testChunkedRequestIsBuffered()
{
    // A request split across two writes must not be answered until its
    // terminating newline arrives, then answered exactly once.
    ServerFixture fixture(QStringLiteral("chunked-request"));
    RawClient client;
    check(client.connect(fixture.name()), "chunked-request client should connect");

    const QByteArray whole = encodeDaemonMessage(makeDaemonRequest(55, daemonMethodName(DaemonMethod::Status)));
    const int split = whole.size() / 2;
    client.sendRaw(whole.left(split));

    // Give the server a chance to (wrongly) respond to the partial frame.
    waitUntil([] { return false; }, 100);
    check(!client.hasBufferedLine(), "an incomplete request must not be answered before its newline arrives");

    client.sendRaw(whole.mid(split));
    const QJsonObject response = client.readResponse();
    check(
        responseId(response) == QStringLiteral("55") && response.value(QStringLiteral("ok")).toBool(false),
        "a request reassembled from chunks must be answered once complete"
    );
    check(!client.hasBufferedLine(), "a chunked request must be answered exactly once");
    client.disconnect();
}

void testDisconnectMidFrameIsCleanedUp()
{
    // A client that sends a partial frame and then vanishes must not wedge the
    // server: it should drop the orphaned buffer and keep serving new clients.
    ServerFixture fixture(QStringLiteral("disconnect-mid-frame"));

    RawClient abandoner;
    check(abandoner.connect(fixture.name()), "mid-frame client should connect");
    abandoner.sendRaw(QByteArrayLiteral("{\"version\":1,\"id\":\"1\",\"method\":\"status\""));
    abandoner.disconnect();

    RawClient fresh;
    check(fresh.connect(fixture.name()), "a fresh client should connect after the abandoned one disconnects");
    fresh.sendRequest(200, daemonMethodName(DaemonMethod::Status));
    const QJsonObject response = fresh.readResponse();
    check(
        responseId(response) == QStringLiteral("200") && response.value(QStringLiteral("ok")).toBool(false),
        "the server must keep serving after a client disconnects mid-frame"
    );
    fresh.disconnect();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    testUnknownMethod();
    testMalformedJson();
    testConnectionSurvivesMalformedRequest();
    testVersionMismatchOnRealMethod();
    testHandshakeIgnoresVersion();
    testEmptyFrameIsSkipped();
    testPipelinedRequestsDrainInOrder();
    testChunkedRequestIsBuffered();
    testDisconnectMidFrameIsCleanedUp();

    if (g_failures != 0) {
        qCritical().noquote() << g_failures << "daemon server failure-path check(s) failed";
        return 1;
    }
    return 0;
}
