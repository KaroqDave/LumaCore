#pragma once

#include "core/DeviceManager.h"

#include <QColor>
#include <QObject>
#include <QStringList>

namespace lumacore {

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(QString profilesDirectory READ profilesDirectory CONSTANT)

public:
    explicit AppController(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString logText() const;
    [[nodiscard]] QString profilesDirectory() const;

    Q_INVOKABLE bool applyStaticColor(int deviceIndex, int zoneIndex, const QColor& color);
    Q_INVOKABLE bool saveProfile(const QString& profileName);
    Q_INVOKABLE bool loadProfile(const QString& profileName);
    Q_INVOKABLE QStringList profileNames() const;

signals:
    void statusMessageChanged();
    void logTextChanged();

private:
    void appendLog(const QString& message);
    void setStatusMessage(const QString& message);

    DeviceManager* m_deviceManager {nullptr};
    QString m_statusMessage;
    QStringList m_logLines;
};

} // namespace lumacore
