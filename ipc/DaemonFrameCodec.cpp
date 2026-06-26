// SPDX-License-Identifier: GPL-2.0-or-later

#include "ipc/DaemonFrameCodec.h"

#include "ipc/DaemonProtocol.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace lumacore {

qint64 daemonFrameReadCapacity(const QByteArray& buffer)
{
    return static_cast<qint64>(kDaemonMaxFrameBytes) - buffer.size();
}

DaemonFrameResult takeNextDaemonFrame(QByteArray* buffer)
{
    if (buffer == nullptr) {
        return {};
    }

    const qsizetype newlineIndex = buffer->indexOf('\n');
    if (newlineIndex < 0) {
        return buffer->size() > kDaemonMaxMessageBytes
            ? DaemonFrameResult {DaemonFrameStatus::OversizedFrame, {}}
            : DaemonFrameResult {};
    }

    if (newlineIndex > kDaemonMaxMessageBytes) {
        return {DaemonFrameStatus::OversizedFrame, {}};
    }

    const QByteArray payload = buffer->left(newlineIndex);
    buffer->remove(0, newlineIndex + 1);
    if (payload.trimmed().isEmpty()) {
        return {DaemonFrameStatus::EmptyFrame, {}};
    }
    return {DaemonFrameStatus::CompleteFrame, payload};
}

bool parseDaemonFrameObject(const QByteArray& payload, QJsonObject* object, QString* errorString)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorString != nullptr) {
            *errorString = parseError.errorString();
        }
        if (object != nullptr) {
            *object = {};
        }
        return false;
    }

    if (object != nullptr) {
        *object = document.object();
    }
    if (errorString != nullptr) {
        errorString->clear();
    }
    return true;
}

} // namespace lumacore
