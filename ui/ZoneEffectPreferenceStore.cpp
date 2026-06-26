// SPDX-License-Identifier: GPL-2.0-or-later

#include "ui/ZoneEffectPreferenceStore.h"

#include "core/RgbDevice.h"

#include <QRegularExpression>
#include <QSettings>
#include <QString>

namespace lumacore {

QString ZoneEffectPreferenceStore::settingsKey(const RgbDevice& device, int zoneIndex)
{
    QString deviceKey = device.id().trimmed();
    deviceKey.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    if (deviceKey.isEmpty()) {
        deviceKey = QStringLiteral("device");
    }
    return QStringLiteral("%1_zone_%2").arg(deviceKey).arg(zoneIndex);
}

bool ZoneEffectPreferenceStore::enabled(const RgbDevice& device, int zoneIndex) const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("ui"));
    settings.beginGroup(QStringLiteral("zoneEffects"));
    const bool value = settings.value(settingsKey(device, zoneIndex), false).toBool();
    settings.endGroup();
    settings.endGroup();
    return value;
}

void ZoneEffectPreferenceStore::setEnabled(const RgbDevice& device, int zoneIndex, bool enabled)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("ui"));
    settings.beginGroup(QStringLiteral("zoneEffects"));
    settings.setValue(settingsKey(device, zoneIndex), enabled);
    settings.endGroup();
    settings.endGroup();
}

} // namespace lumacore
