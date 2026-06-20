#pragma once

#include "core/BackendCapabilities.h"
#include "core/RgbColor.h"
#include "core/RgbEffect.h"
#include "core/RgbZone.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <optional>

namespace lumacore {

enum class RgbDeviceType {
    Motherboard,
    Controller,
    Unknown
};

[[nodiscard]] QString rgbDeviceTypeToString(RgbDeviceType type);
[[nodiscard]] RgbDeviceType rgbDeviceTypeFromString(const QString& value);

class RgbDevice : public QObject
{
    Q_OBJECT

public:
    RgbDevice(QString id, QString name, QString vendor, RgbDeviceType type, QObject* parent = nullptr);
    ~RgbDevice() override = default;

    [[nodiscard]] const QString& id() const;
    [[nodiscard]] const QString& name() const;
    [[nodiscard]] const QString& vendor() const;
    [[nodiscard]] const QString& backendId() const;
    void setBackendId(const QString& backendId);
    [[nodiscard]] virtual QString discoveryIdentity() const;
    [[nodiscard]] RgbDeviceType type() const;
    [[nodiscard]] QString typeName() const;
    [[nodiscard]] const QVector<RgbZone>& zones() const;
    [[nodiscard]] bool likelyRgbController() const;
    void setLikelyRgbController(bool likelyRgbController);
    [[nodiscard]] bool hasRgbControllerOverride() const;
    [[nodiscard]] bool rgbControllerOverride() const;
    void setRgbControllerOverride(bool isRgbController);
    void clearRgbControllerOverride();
    [[nodiscard]] bool isRgbController() const;

    [[nodiscard]] bool setZoneName(int zoneIndex, const QString& name);
    [[nodiscard]] bool setZoneLedCount(int zoneIndex, int ledCount);
    [[nodiscard]] virtual bool setZoneStaticColor(int zoneIndex, const RgbColor& color) = 0;
    [[nodiscard]] virtual bool applyZoneEffect(int zoneIndex, const RgbEffect& effect);
    [[nodiscard]] virtual bool applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors);
    [[nodiscard]] virtual bool applyAllOff();
    [[nodiscard]] virtual bool updateZoneMetadata(int zoneIndex, const QString& name, int ledCount);
    [[nodiscard]] virtual bool usesLocalFrameRendering() const;
    [[nodiscard]] virtual QString previewZoneEffectWrite(int zoneIndex, const RgbEffect& effect) const;
    [[nodiscard]] virtual QString lastHardwareWriteStatus() const;
    [[nodiscard]] virtual BackendCapabilities capabilities() const;
    [[nodiscard]] virtual PermissionResult checkRuntimePermission(BackendCapability capability) const;

    void setZoneEffect(int zoneIndex, const RgbEffect& effect);
    [[nodiscard]] RgbEffect zoneEffect(int zoneIndex) const;
    [[nodiscard]] virtual bool setZoneEffectColors(int zoneIndex, const QVector<RgbColor>& colors);

signals:
    void zoneChanged(int zoneIndex);

protected:
    [[nodiscard]] QVector<RgbZone>& mutableZones();

private:
    QString m_id;
    QString m_name;
    QString m_vendor;
    QString m_backendId;
    RgbDeviceType m_type {RgbDeviceType::Unknown};
    QVector<RgbZone> m_zones;
    bool m_likelyRgbController {false};
    std::optional<bool> m_rgbControllerOverride;
};

} // namespace lumacore
