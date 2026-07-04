// SPDX-License-Identifier: GPL-2.0-or-later

// Direct unit coverage for the daemon frame codec, the trust boundary that
// turns an untrusted byte stream from the local socket into discrete JSON
// request/response frames. DaemonServer and DaemonClient both drive
// takeNextDaemonFrame()/parseDaemonFrameObject() in a read loop, so the codec
// must survive chunked delivery, back-to-back frames, oversized payloads at the
// exact size boundary, and arbitrary binary garbage without ever crashing or
// mis-slicing a frame. Those cases were previously exercised only indirectly
// with well-formed input through the end-to-end test.

#include "ipc/DaemonFrameCodec.h"
#include "ipc/DaemonProtocol.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QString>

#include <vector>

namespace {

int g_failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << "FAIL:" << message;
        ++g_failures;
    }
}

using namespace lumacore;

// Drain a buffer the way the real consumers do: repeatedly take frames until the
// codec asks for more data, collecting every complete payload in order. Returns
// the terminal status (NeedMoreData unless an oversized/empty frame stops us) so
// callers can assert on how the stream ended.
DaemonFrameStatus drainFrames(QByteArray* buffer, std::vector<QByteArray>* payloads)
{
    while (true) {
        const DaemonFrameResult frame = takeNextDaemonFrame(buffer);
        if (frame.status == DaemonFrameStatus::CompleteFrame) {
            payloads->push_back(frame.payload);
            continue;
        }
        if (frame.status == DaemonFrameStatus::EmptyFrame) {
            continue;
        }
        return frame.status;
    }
}

void testNullBuffer()
{
    const DaemonFrameResult frame = takeNextDaemonFrame(nullptr);
    check(
        frame.status == DaemonFrameStatus::NeedMoreData && frame.payload.isEmpty(),
        "a null buffer pointer must yield NeedMoreData without crashing"
    );
}

void testEmptyAndPartialBuffer()
{
    QByteArray empty;
    check(
        takeNextDaemonFrame(&empty).status == DaemonFrameStatus::NeedMoreData,
        "an empty buffer needs more data"
    );

    QByteArray partial = QByteArrayLiteral("{\"id\":\"1\"}");
    const DaemonFrameResult frame = takeNextDaemonFrame(&partial);
    check(
        frame.status == DaemonFrameStatus::NeedMoreData,
        "a payload without a trailing newline is incomplete"
    );
    check(
        partial == QByteArrayLiteral("{\"id\":\"1\"}"),
        "an incomplete read must leave the buffer untouched for the next chunk"
    );
}

void testSingleCompleteFrame()
{
    QByteArray buffer = QByteArrayLiteral("{\"id\":\"1\"}\n");
    const DaemonFrameResult frame = takeNextDaemonFrame(&buffer);
    check(
        frame.status == DaemonFrameStatus::CompleteFrame,
        "a newline-terminated payload is a complete frame"
    );
    check(
        frame.payload == QByteArrayLiteral("{\"id\":\"1\"}"),
        "the complete frame payload must exclude the trailing newline"
    );
    check(buffer.isEmpty(), "consuming the only frame must drain the buffer");
}

void testMultipleFramesInOneBuffer()
{
    QByteArray buffer = QByteArrayLiteral("{\"a\":1}\n{\"b\":2}\n{\"c\":3}\n");
    std::vector<QByteArray> payloads;
    const DaemonFrameStatus terminal = drainFrames(&buffer, &payloads);
    check(terminal == DaemonFrameStatus::NeedMoreData, "a fully drained buffer ends in NeedMoreData");
    check(payloads.size() == 3, "three newline-delimited frames must all be extracted");
    check(
        payloads.size() == 3 && payloads[0] == QByteArrayLiteral("{\"a\":1}")
            && payloads[1] == QByteArrayLiteral("{\"b\":2}")
            && payloads[2] == QByteArrayLiteral("{\"c\":3}"),
        "back-to-back frames must be returned in arrival order"
    );
    check(buffer.isEmpty(), "draining every frame must empty the buffer");
}

void testTrailingRemainderAfterFrames()
{
    QByteArray buffer = QByteArrayLiteral("{\"a\":1}\n{\"partial\"");
    std::vector<QByteArray> payloads;
    const DaemonFrameStatus terminal = drainFrames(&buffer, &payloads);
    check(terminal == DaemonFrameStatus::NeedMoreData, "an unfinished trailing frame stops at NeedMoreData");
    check(
        payloads.size() == 1 && payloads[0] == QByteArrayLiteral("{\"a\":1}"),
        "complete frames are yielded even when a partial frame trails them"
    );
    check(
        buffer == QByteArrayLiteral("{\"partial\""),
        "the unfinished remainder must stay buffered for the next chunk"
    );
}

void testChunkedReassembly()
{
    // Simulate a frame split across three socket reads that also carries the
    // start of the next frame in the final chunk.
    QByteArray buffer;
    std::vector<QByteArray> payloads;

    buffer.append(QByteArrayLiteral("{\"long"));
    check(drainFrames(&buffer, &payloads) == DaemonFrameStatus::NeedMoreData, "chunk 1 is incomplete");
    check(payloads.empty(), "no frame completes from the first chunk");

    buffer.append(QByteArrayLiteral("\":\"value\""));
    check(drainFrames(&buffer, &payloads) == DaemonFrameStatus::NeedMoreData, "chunk 2 is still incomplete");
    check(payloads.empty(), "no frame completes from the second chunk");

    buffer.append(QByteArrayLiteral("}\n{\"next\":1"));
    check(drainFrames(&buffer, &payloads) == DaemonFrameStatus::NeedMoreData, "the tail of chunk 3 is a new partial frame");
    check(
        payloads.size() == 1 && payloads[0] == QByteArrayLiteral("{\"long\":\"value\"}"),
        "a frame split across reads must reassemble byte-for-byte"
    );
    check(
        buffer == QByteArrayLiteral("{\"next\":1"),
        "the start of the following frame must remain buffered"
    );
}

void testEmptyFrames()
{
    QByteArray buffer = QByteArrayLiteral("\n");
    const DaemonFrameResult bare = takeNextDaemonFrame(&buffer);
    check(bare.status == DaemonFrameStatus::EmptyFrame, "a lone newline is an empty frame");
    check(buffer.isEmpty(), "an empty frame still advances the buffer past its newline");

    QByteArray whitespace = QByteArrayLiteral("   \t \n{\"id\":\"1\"}\n");
    std::vector<QByteArray> payloads;
    const DaemonFrameStatus terminal = drainFrames(&whitespace, &payloads);
    check(terminal == DaemonFrameStatus::NeedMoreData, "whitespace framing drains cleanly");
    check(
        payloads.size() == 1 && payloads[0] == QByteArrayLiteral("{\"id\":\"1\"}"),
        "a whitespace-only frame is skipped while the following real frame survives"
    );
}

void testWhitespacePaddedPayloadIsPreserved()
{
    // Only the emptiness check trims; a non-empty payload must be handed back
    // verbatim (leading/trailing space included) so the JSON parser sees exactly
    // what arrived on the wire.
    QByteArray buffer = QByteArrayLiteral("  {\"id\":\"1\"}  \n");
    const DaemonFrameResult frame = takeNextDaemonFrame(&buffer);
    check(frame.status == DaemonFrameStatus::CompleteFrame, "a padded but non-empty payload is a complete frame");
    check(
        frame.payload == QByteArrayLiteral("  {\"id\":\"1\"}  "),
        "surrounding whitespace must be preserved in the returned payload"
    );
}

void testFirstNewlineWins()
{
    // indexOf finds the first newline; a payload can never straddle one.
    QByteArray buffer = QByteArrayLiteral("{\"a\":1}\n\n{\"b\":2}\n");
    std::vector<QByteArray> payloads;
    drainFrames(&buffer, &payloads);
    check(
        payloads.size() == 2 && payloads[0] == QByteArrayLiteral("{\"a\":1}")
            && payloads[1] == QByteArrayLiteral("{\"b\":2}"),
        "an interior blank line splits into an empty frame, not a merged payload"
    );
}

void testOversizedWithoutNewline()
{
    // Exactly the max message size with no delimiter is still just an
    // in-progress read; one byte more is a hard oversized rejection.
    QByteArray atLimit(kDaemonMaxMessageBytes, 'x');
    check(
        takeNextDaemonFrame(&atLimit).status == DaemonFrameStatus::NeedMoreData,
        "a delimiterless buffer at exactly the max message size still awaits a newline"
    );

    QByteArray overLimit(kDaemonMaxMessageBytes + 1, 'x');
    const DaemonFrameResult frame = takeNextDaemonFrame(&overLimit);
    check(
        frame.status == DaemonFrameStatus::OversizedFrame,
        "a delimiterless buffer past the max message size is rejected as oversized"
    );
    check(
        overLimit.size() == kDaemonMaxMessageBytes + 1,
        "an oversized rejection leaves the buffer for the caller to clear"
    );
}

void testOversizedWithNewline()
{
    // A payload of exactly the max size (newline at index kDaemonMaxMessageBytes)
    // is accepted; pushing the newline one byte further is oversized.
    QByteArray maxPayload(kDaemonMaxMessageBytes, 'x');
    maxPayload.append('\n');
    const DaemonFrameResult accepted = takeNextDaemonFrame(&maxPayload);
    check(
        accepted.status == DaemonFrameStatus::CompleteFrame
            && accepted.payload.size() == kDaemonMaxMessageBytes,
        "a payload of exactly the max message size is delivered intact"
    );

    QByteArray tooLong(kDaemonMaxMessageBytes + 1, 'x');
    tooLong.append('\n');
    const DaemonFrameResult rejected = takeNextDaemonFrame(&tooLong);
    check(
        rejected.status == DaemonFrameStatus::OversizedFrame,
        "a newline one byte past the max message size is oversized"
    );
    check(
        tooLong.size() == kDaemonMaxMessageBytes + 2,
        "an oversized delimited frame is not consumed; the caller must clear it"
    );
}

void testBinaryGarbageIsSafe()
{
    // Arbitrary non-UTF-8 bytes must never crash the codec. Without a newline it
    // is merely incomplete; with one it produces a frame that fails JSON parsing
    // cleanly rather than being mistaken for a valid request.
    QByteArray garbage;
    for (int i = 0; i < 256; ++i) {
        if (i != '\n') {
            garbage.append(static_cast<char>(i));
        }
    }
    QByteArray noNewline = garbage;
    check(
        takeNextDaemonFrame(&noNewline).status == DaemonFrameStatus::NeedMoreData,
        "binary bytes without a newline are treated as an incomplete frame"
    );

    QByteArray delimited = garbage;
    delimited.append('\n');
    const DaemonFrameResult frame = takeNextDaemonFrame(&delimited);
    check(
        frame.status == DaemonFrameStatus::CompleteFrame,
        "a newline-terminated binary blob is extracted as a frame"
    );
    QJsonObject object;
    QString error;
    check(
        !parseDaemonFrameObject(frame.payload, &object, &error),
        "binary garbage must fail JSON object parsing"
    );
    check(!error.isEmpty(), "a parse failure must report an error string");
    check(object.isEmpty(), "a failed parse must clear the output object");
}

void testParseValidObject()
{
    QJsonObject object;
    QString error;
    const bool ok = parseDaemonFrameObject(QByteArrayLiteral("{\"id\":\"7\",\"ok\":true}"), &object, &error);
    check(ok, "a well-formed JSON object must parse");
    check(error.isEmpty(), "a successful parse must clear the error string");
    check(
        object.value(QStringLiteral("id")).toString() == QStringLiteral("7")
            && object.value(QStringLiteral("ok")).toBool(),
        "parsed fields must round-trip"
    );
}

void testParseRejectsNonObjectJson()
{
    // Valid JSON that is not an object (arrays, bare scalars) must be rejected:
    // the protocol only ever exchanges objects.
    QJsonObject object;
    QString error;
    check(
        !parseDaemonFrameObject(QByteArrayLiteral("[1,2,3]"), &object, &error),
        "a JSON array is not a valid request/response object"
    );
    check(
        !parseDaemonFrameObject(QByteArrayLiteral("42"), &object, nullptr),
        "a bare JSON scalar is not a valid object"
    );
    check(
        !parseDaemonFrameObject(QByteArrayLiteral("\"text\""), nullptr, &error),
        "a bare JSON string is not a valid object even with a null object out-parameter"
    );
    check(
        !parseDaemonFrameObject(QByteArrayLiteral(""), &object, &error),
        "an empty payload is not a valid object"
    );
}

void testParseHandlesNullOutParameters()
{
    // The codec accepts null object/error pointers; callers that only care about
    // success must not crash.
    check(
        parseDaemonFrameObject(QByteArrayLiteral("{}"), nullptr, nullptr),
        "an empty object parses successfully even with null out-parameters"
    );
    check(
        !parseDaemonFrameObject(QByteArrayLiteral("{oops"), nullptr, nullptr),
        "malformed JSON fails cleanly with null out-parameters"
    );
}

void testReadCapacity()
{
    QByteArray empty;
    check(
        daemonFrameReadCapacity(empty) == static_cast<qint64>(kDaemonMaxFrameBytes),
        "an empty buffer can absorb a full frame's worth of bytes"
    );

    QByteArray partial(1000, 'x');
    check(
        daemonFrameReadCapacity(partial) == static_cast<qint64>(kDaemonMaxFrameBytes) - 1000,
        "remaining capacity shrinks by the buffered byte count"
    );

    QByteArray full(kDaemonMaxFrameBytes, 'x');
    check(
        daemonFrameReadCapacity(full) == 0,
        "a buffer holding a full frame has no remaining capacity, halting further reads"
    );
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    testNullBuffer();
    testEmptyAndPartialBuffer();
    testSingleCompleteFrame();
    testMultipleFramesInOneBuffer();
    testTrailingRemainderAfterFrames();
    testChunkedReassembly();
    testEmptyFrames();
    testWhitespacePaddedPayloadIsPreserved();
    testFirstNewlineWins();
    testOversizedWithoutNewline();
    testOversizedWithNewline();
    testBinaryGarbageIsSafe();
    testParseValidObject();
    testParseRejectsNonObjectJson();
    testParseHandlesNullOutParameters();
    testReadCapacity();

    if (g_failures != 0) {
        qCritical().noquote() << g_failures << "frame codec check(s) failed";
        return 1;
    }
    return 0;
}
