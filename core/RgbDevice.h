#pragma once

#include "core/RgbColor.h"
#include "core/RgbZone.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace lumacore {

class RgbDevice : public QObject
{
    Q_OBJECT

public:
    RgbDevice(QString id, QString name, QObject* parent = nullptr);
    ~RgbDevice() override = default;

    [[nodiscard]] const QString& id() const;
    [[nodiscard]] const QString& name() const;
    [[nodiscard]] const QVector<RgbZone>& zones() const;

    [[nodiscard]] virtual bool setZoneStaticColor(int zoneIndex, const RgbColor& color) = 0;

signals:
    void zoneChanged(int zoneIndex);

protected:
    [[nodiscard]] QVector<RgbZone>& mutableZones();

private:
    QString m_id;
    QString m_name;
    QVector<RgbZone> m_zones;
};

} // namespace lumacore
