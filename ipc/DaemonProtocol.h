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
[[nodiscard]] QJsonObject zoneToJson(const RgbZone& zone);
[[nodiscard]] QJsonObject permissionResultsToJson(const RgbDevice& device);
[[nodiscard]] QJsonObject deviceToJson(const RgbDevice& device, int index, bool writeConfirmed = false);
[[nodiscard]] RgbEffect effectFromJson(const QJsonObject& object);

} // namespace lumacore
