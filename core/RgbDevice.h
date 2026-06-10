#pragma once

#include "core/RgbColor.h"
#include "core/RgbZone.h"

#include <QObject>
#include <QString>
#include <QVector>

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
    [[nodiscard]] RgbDeviceType type() const;
    [[nodiscard]] QString typeName() const;
    [[nodiscard]] const QVector<RgbZone>& zones() const;

    [[nodiscard]] bool setZoneName(int zoneIndex, const QString& name);
    [[nodiscard]] bool setZoneLedCount(int zoneIndex, int ledCount);
    [[nodiscard]] virtual bool setZoneStaticColor(int zoneIndex, const RgbColor& color) = 0;

signals:
    void zoneChanged(int zoneIndex);

protected:
    [[nodiscard]] QVector<RgbZone>& mutableZones();

private:
    QString m_id;
    QString m_name;
    QString m_vendor;
    RgbDeviceType m_type {RgbDeviceType::Unknown};
    QVector<RgbZone> m_zones;
};

} // namespace lumacore
