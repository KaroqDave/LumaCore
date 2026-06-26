// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace lumacore {

enum class DaemonFrameStatus {
    NeedMoreData,
    EmptyFrame,
    CompleteFrame,
    OversizedFrame,
};

struct DaemonFrameResult {
    DaemonFrameStatus status {DaemonFrameStatus::NeedMoreData};
    QByteArray payload;
};

[[nodiscard]] qint64 daemonFrameReadCapacity(const QByteArray& buffer);
[[nodiscard]] DaemonFrameResult takeNextDaemonFrame(QByteArray* buffer);
[[nodiscard]] bool parseDaemonFrameObject(const QByteArray& payload, QJsonObject* object, QString* errorString);

} // namespace lumacore
