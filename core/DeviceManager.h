#pragma once

#include "core/EffectsEngine.h"
#include "core/RgbColor.h"
#include "core/RgbDevice.h"
#include "core/RgbEffect.h"

#include <QObject>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

namespace lumacore {

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject* parent = nullptr);

    void initializeMockDevices();

    [[nodiscard]] int deviceCount() const;
    [[nodiscard]] RgbDevice* deviceAt(int index);
    [[nodiscard]] const RgbDevice* deviceAt(int index) const;
    [[nodiscard]] const std::vector<std::unique_ptr<RgbDevice>>& devices() const;

    [[nodiscard]] bool updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount, QString* errorMessage = nullptr);
    [[nodiscard]] bool setZoneStaticColor(int deviceIndex, int zoneIndex, const RgbColor& color);
    [[nodiscard]] bool applyZoneEffect(int deviceIndex, int zoneIndex, const RgbEffect& effect);
    void paintZoneFrame(int deviceIndex, int zoneIndex, const QVector<RgbColor>& colors);
    [[nodiscard]] bool saveProfile(const QString& profileName, QString* errorMessage = nullptr);
    [[nodiscard]] bool loadProfile(const QString& profileName, QString* errorMessage = nullptr);
    [[nodiscard]] bool deleteProfile(const QString& profileName, QString* errorMessage = nullptr);
    [[nodiscard]] bool renameProfile(const QString& oldProfileName, const QString& newProfileName, QString* errorMessage = nullptr);
    [[nodiscard]] QStringList profileNames() const;
    [[nodiscard]] QString profilesDirectoryPath() const;

signals:
    void devicesChanged();
    void zoneChanged(int deviceIndex, int zoneIndex);
    void zoneColorChanged(int deviceIndex, int zoneIndex);
    void zoneFrameUpdated(int deviceIndex, int zoneIndex);
    void logMessage(QString message);

private:
    void registerDevice(std::unique_ptr<RgbDevice> device);
    [[nodiscard]] QString profileFilePath(const QString& profileName) const;
    [[nodiscard]] static QString normalizeProfileName(const QString& profileName);

    std::vector<std::unique_ptr<RgbDevice>> m_devices;
    std::unique_ptr<EffectsEngine> m_effectsEngine;
};

} // namespace lumacore
