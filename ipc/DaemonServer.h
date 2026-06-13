#pragma once

#include "core/DeviceManager.h"

#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QString>

namespace lumacore {

class DaemonServer final : public QObject
{
    Q_OBJECT

public:
    explicit DaemonServer(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] bool listen(const QString& socketPath, QString* errorMessage = nullptr);
    void close();
    [[nodiscard]] QString socketPath() const;

private:
    void handleNewConnection();
    void handleReadyRead(QLocalSocket* socket);
    void handleDisconnected(QLocalSocket* socket);

    [[nodiscard]] QJsonObject handleRequest(const QJsonObject& request);
    [[nodiscard]] QJsonObject statusPayload() const;
    [[nodiscard]] QJsonObject listDevicesPayload() const;
    [[nodiscard]] QJsonObject previewEffect(const QJsonObject& params) const;
    [[nodiscard]] QJsonObject applyEffect(const QJsonObject& params);
    [[nodiscard]] QJsonObject updateZone(const QJsonObject& params);
    [[nodiscard]] QJsonObject confirmWrites(const QJsonObject& params);
    [[nodiscard]] QJsonObject revokeWrites(const QJsonObject& params);
    [[nodiscard]] QJsonObject allOff(const QJsonObject& params);
    [[nodiscard]] QJsonObject setDryRun(const QJsonObject& params);
    [[nodiscard]] QJsonObject activityLogSnapshot() const;

    DeviceManager* m_deviceManager {nullptr};
    QLocalServer m_server;
    QString m_socketPath;
    QHash<QLocalSocket*, QByteArray> m_buffers;
};

} // namespace lumacore
