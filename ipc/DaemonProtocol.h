// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/BackendCapabilities.h"
#include "core/RgbBackend.h"
#include "core/RgbDevice.h"
#include "core/RgbEffect.h"
#include "core/RgbZone.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace lumacore {

inline constexpr int kDaemonProtocolVersion = 1;
inline constexpr int kDaemonMaxMessageBytes = 1024 * 1024;
inline constexpr int kDaemonMaxFrameBytes = kDaemonMaxMessageBytes + 1;

enum class DaemonMethod {
    Unknown,
    Hello,
    Status,
    ListDevices,
    PreviewEffect,
    ApplyEffect,
    UpdateZone,
    ConfirmWrites,
    RevokeWrites,
    AllOff,
    PaintZoneFrame,
    SetDryRun,
    ActivityLogSnapshot,
};

[[nodiscard]] QString daemonMethodName(DaemonMethod method);
[[nodiscard]] DaemonMethod daemonMethodFromName(const QString& name);

[[nodiscard]] QString defaultDaemonSocketPath();

[[nodiscard]] QByteArray encodeDaemonMessage(const QJsonObject& message);
[[nodiscard]] QJsonObject makeDaemonRequest(quint64 id, const QString& method, const QJsonObject& params = {});
[[nodiscard]] QJsonObject makeDaemonResult(quint64 id, const QJsonObject& result = {});
[[nodiscard]] QJsonObject makeDaemonError(quint64 id, const QString& message);

[[nodiscard]] QString permissionStatusToString(PermissionStatus status);
[[nodiscard]] PermissionStatus permissionStatusFromString(const QString& value);
[[nodiscard]] QJsonArray capabilitiesToJson(BackendCapabilities capabilities);
[[nodiscard]] BackendCapabilities capabilitiesFromJson(const QJsonArray& values);
[[nodiscard]] QJsonObject permissionResultToJson(const PermissionResult& result);
[[nodiscard]] PermissionResult permissionResultFromJson(const QJsonObject& object);

[[nodiscard]] QJsonObject backendDescriptorToJson(const BackendDescriptor& descriptor);
[[nodiscard]] BackendDescriptor backendDescriptorFromJson(const QJsonObject& object);
[[nodiscard]] QJsonObject zoneToJson(const RgbDevice& device, int zoneIndex);
[[nodiscard]] QJsonObject permissionResultsToJson(const RgbDevice& device);
[[nodiscard]] QJsonObject deviceToJson(
    const RgbDevice& device,
    int index,
    bool writeConfirmed = false
);
[[nodiscard]] RgbEffect effectFromJson(const QJsonObject& object);
[[nodiscard]] QJsonArray colorsToJson(const QVector<RgbColor>& colors);
[[nodiscard]] QVector<RgbColor> colorsFromJson(const QJsonArray& values);

} // namespace lumacore
