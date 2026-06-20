#pragma once

#include "core/RgbDevice.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace lumacore::hardware::linux {

struct ProbeDevice {
    QString source;
    QString id;
    QString name;
    QString vendor;
    QString path;
    QString details;
    QString vendorId;
    QString productId;
    int interfaceNumber {-1};
    quint16 usagePage {0};
    quint16 usage {0};
    RgbDeviceType type {RgbDeviceType::Unknown};
};

struct ProbeResult {
    QString providerName;
    bool providerAvailable {false};
    QVector<ProbeDevice> devices;
    QStringList messages;
};

[[nodiscard]] QString stableProbeId(const QString& source, const QString& key);
[[nodiscard]] QString hexWord(quint16 value);
[[nodiscard]] QString usbVidPidKey(const ProbeDevice& device);
[[nodiscard]] bool isKnownRgbController(const ProbeDevice& device);
[[nodiscard]] bool isLikelyRgbController(const ProbeDevice& device);

} // namespace lumacore::hardware::linux
