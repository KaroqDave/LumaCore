#pragma once

#include "core/ActivityLog.h"
#include "core/BackendRegistry.h"
#include "core/EffectsEngine.h"
#include "core/ProfileStore.h"
#include "core/RgbColor.h"
#include "core/RgbDevice.h"
#include "core/RgbEffect.h"

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <memory>
#include <vector>

namespace lumacore {

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject* parent = nullptr, QString profilesDirectory = {});

    void registerBackend(std::unique_ptr<RgbBackend> backend);
    [[nodiscard]] bool activateBackend(const QString& id);
    void initializeBackends(const QString& backendId = {});
    void initializeMockDevices();

    [[nodiscard]] bool dryRunEnabled() const;
    void setDryRunEnabled(bool enabled);

    [[nodiscard]] int deviceCount() const;
    [[nodiscard]] RgbDevice* deviceAt(int index);
    [[nodiscard]] const RgbDevice* deviceAt(int index) const;
    [[nodiscard]] const std::vector<std::unique_ptr<RgbDevice>>& devices() const;
    [[nodiscard]] bool markDeviceRgbController(int deviceIndex, bool isRgbController);
    [[nodiscard]] bool clearDeviceRgbControllerOverride(int deviceIndex);
    [[nodiscard]] bool confirmDeviceWrites(int deviceIndex);
    [[nodiscard]] bool revokeDeviceWrites(int deviceIndex);
    [[nodiscard]] bool deviceWriteConfirmed(int deviceIndex) const;

    [[nodiscard]] bool updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount, QString* errorMessage = nullptr);
    [[nodiscard]] bool setZoneStaticColor(int deviceIndex, int zoneIndex, const RgbColor& color);
    [[nodiscard]] bool applyZoneEffect(int deviceIndex, int zoneIndex, const RgbEffect& effect);
    [[nodiscard]] bool applyAllOff(int deviceIndex, QString* errorMessage = nullptr);
    void paintZoneFrame(int deviceIndex, int zoneIndex, const QVector<RgbColor>& colors);
    [[nodiscard]] bool saveProfile(const QString& profileName, QString* errorMessage = nullptr);
    [[nodiscard]] bool loadProfile(const QString& profileName, QString* errorMessage = nullptr);
    [[nodiscard]] QVariantMap applyProfileWithReport(const QString& profileName);
    [[nodiscard]] bool deleteProfile(const QString& profileName, QString* errorMessage = nullptr);
    [[nodiscard]] bool renameProfile(const QString& oldProfileName, const QString& newProfileName, QString* errorMessage = nullptr);
    [[nodiscard]] bool importProfile(const QString& sourcePath, QString* importedProfileName = nullptr, QString* errorMessage = nullptr);
    [[nodiscard]] bool exportProfile(const QString& profileName, const QString& destinationPath, QString* errorMessage = nullptr);
    [[nodiscard]] QVariantMap profileCompatibility(const QString& profileName) const;
    [[nodiscard]] QStringList profileNames() const;
    [[nodiscard]] QString profilesDirectoryPath() const;
    [[nodiscard]] ActivityLog& activityLog();
    [[nodiscard]] const ActivityLog& activityLog() const;
    [[nodiscard]] BackendRegistry& backendRegistry();
    [[nodiscard]] const BackendRegistry& backendRegistry() const;

signals:
    void devicesChanged();
    void zoneChanged(int deviceIndex, int zoneIndex);
    void zoneColorChanged(int deviceIndex, int zoneIndex);
    void zoneFrameUpdated(int deviceIndex, int zoneIndex);
    void logMessage(QString message);
    void dryRunEnabledChanged();

private:
    [[nodiscard]] RgbDevice* deviceForZone(int deviceIndex, int zoneIndex);
    void registerDevice(std::unique_ptr<RgbDevice> device);
    void applySavedRgbControllerOverride(RgbDevice& device);
    void saveRgbControllerOverride(const RgbDevice& device);
    void removeRgbControllerOverride(const RgbDevice& device);
    [[nodiscard]] static QString rgbControllerOverrideKey(const QString& deviceId);
    [[nodiscard]] static QString normalizeProfileName(const QString& profileName);

    std::vector<std::unique_ptr<RgbDevice>> m_devices;
    std::unique_ptr<EffectsEngine> m_effectsEngine;
    ActivityLog m_activityLog;
    BackendRegistry m_backendRegistry;
    ProfileStore m_profileStore;
    QSet<QString> m_confirmedWriteDeviceIds;
    bool m_dryRunEnabled {false};
};

} // namespace lumacore
