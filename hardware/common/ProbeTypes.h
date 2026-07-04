// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbDevice.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace lumacore::hardware::common {

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

struct DiscoverySupportInfo {
    QString stage;
    QString status;
    QString family;
    QString summary;
    QString notes;
    bool cataloged {false};
    bool likelyRgbController {false};
    bool writeCapableBackend {false};
};

} // namespace lumacore::hardware::common
