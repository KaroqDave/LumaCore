#include "core/RgbDevice.h"

#include <utility>

namespace lumacore {

QString rgbDeviceTypeToString(RgbDeviceType type)
{
    switch (type) {
    case RgbDeviceType::Motherboard:
        return QStringLiteral("Motherboard");
    case RgbDeviceType::Controller:
        return QStringLiteral("Controller");
    case RgbDeviceType::Unknown:
        return QStringLiteral("Unknown");
    }

    return QStringLiteral("Unknown");
}

RgbDeviceType rgbDeviceTypeFromString(const QString& value)
{
    if (value.compare(QStringLiteral("Motherboard"), Qt::CaseInsensitive) == 0) {
        return RgbDeviceType::Motherboard;
    }

    if (value.compare(QStringLiteral("Controller"), Qt::CaseInsensitive) == 0) {
        return RgbDeviceType::Controller;
    }

    return RgbDeviceType::Unknown;
}

RgbDevice::RgbDevice(QString id, QString name, QString vendor, RgbDeviceType type, QObject* parent)
    : QObject(parent)
    , m_id(std::move(id))
    , m_name(std::move(name))
    , m_vendor(std::move(vendor))
    , m_type(type)
{
}

const QString& RgbDevice::id() const
{
    return m_id;
}

const QString& RgbDevice::name() const
{
    return m_name;
}

const QString& RgbDevice::vendor() const
{
    return m_vendor;
}

const QString& RgbDevice::backendId() const
{
    return m_backendId;
}

void RgbDevice::setBackendId(const QString& backendId)
{
    m_backendId = backendId;
}

QString RgbDevice::discoveryIdentity() const
{
    return {};
}

RgbDeviceType RgbDevice::type() const
{
    return m_type;
}

QString RgbDevice::typeName() const
{
    return rgbDeviceTypeToString(m_type);
}

const QVector<RgbZone>& RgbDevice::zones() const
{
    return m_zones;
}

bool RgbDevice::likelyRgbController() const
{
    return m_likelyRgbController;
}

void RgbDevice::setLikelyRgbController(bool likelyRgbController)
{
    m_likelyRgbController = likelyRgbController;
}

bool RgbDevice::hasRgbControllerOverride() const
{
    return m_rgbControllerOverride.has_value();
}

bool RgbDevice::rgbControllerOverride() const
{
    return m_rgbControllerOverride.value_or(false);
}

void RgbDevice::setRgbControllerOverride(bool isRgbController)
{
    m_rgbControllerOverride = isRgbController;
}

void RgbDevice::clearRgbControllerOverride()
{
    m_rgbControllerOverride.reset();
}

bool RgbDevice::isRgbController() const
{
    return m_rgbControllerOverride.value_or(m_likelyRgbController);
}

bool RgbDevice::setZoneName(int zoneIndex, const QString& name)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size()) {
        return false;
    }

    const QString sanitizedName = name.trimmed();
    if (sanitizedName.isEmpty() || m_zones.at(zoneIndex).name() == sanitizedName) {
        return false;
    }

    m_zones[zoneIndex].setName(sanitizedName);
    emit zoneChanged(zoneIndex);
    return true;
}

bool RgbDevice::setZoneLedCount(int zoneIndex, int ledCount)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size() || ledCount < 0 || m_zones.at(zoneIndex).ledCount() == ledCount) {
        return false;
    }

    m_zones[zoneIndex].setLedCount(ledCount);
    emit zoneChanged(zoneIndex);
    return true;
}

bool RgbDevice::applyZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size()) {
        return false;
    }

    setZoneEffect(zoneIndex, effect);
    return true;
}

bool RgbDevice::applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors)
{
    return setZoneEffectColors(zoneIndex, colors);
}

bool RgbDevice::applyAllOff()
{
    return false;
}

bool RgbDevice::updateZoneMetadata(int zoneIndex, const QString& name, int ledCount)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size()) {
        return false;
    }

    const bool nameChanged = setZoneName(zoneIndex, name);
    const bool ledCountChanged = setZoneLedCount(zoneIndex, ledCount);
    return nameChanged || ledCountChanged;
}

bool RgbDevice::usesLocalFrameRendering() const
{
    return true;
}

QString RgbDevice::previewZoneEffectWrite(int zoneIndex, const RgbEffect& effect) const
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(effect)
    return {};
}

QString RgbDevice::lastHardwareWriteStatus() const
{
    return {};
}

void RgbDevice::setZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size()) {
        return;
    }

    m_zones[zoneIndex].setEffect(effect);
}

RgbEffect RgbDevice::zoneEffect(int zoneIndex) const
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size()) {
        return {};
    }

    return m_zones.at(zoneIndex).effect();
}

bool RgbDevice::setZoneEffectColors(int zoneIndex, const QVector<RgbColor>& colors)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.size() || colors.isEmpty()) {
        return false;
    }

    m_zones[zoneIndex].setLedColors(colors);
    return true;
}

QVector<RgbZone>& RgbDevice::mutableZones()
{
    return m_zones;
}

BackendCapabilities RgbDevice::capabilities() const
{
    return BackendCapability::None;
}

PermissionResult RgbDevice::checkRuntimePermission(BackendCapability capability) const
{
    Q_UNUSED(capability)
    return {
        PermissionStatus::Denied,
        QStringLiteral("No backend capabilities declared for %1.").arg(name()),
    };
}

bool RgbDevice::supportsEffect(int effectType) const
{
    if (effectType < static_cast<int>(RgbEffectType::Static)
        || effectType > static_cast<int>(RgbEffectType::ColorCycle)) {
        return false;
    }

    const bool animated = effectType != static_cast<int>(RgbEffectType::Static);
    return capabilities().testFlag(
        animated ? BackendCapability::ZoneEffectWrite : BackendCapability::ZoneColorWrite
    );
}

bool RgbDevice::supportsEffectSpeed(int effectType) const
{
    return effectType != static_cast<int>(RgbEffectType::Static) && supportsEffect(effectType);
}

bool RgbDevice::supportsEffectBrightness(int effectType) const
{
    return supportsEffect(effectType);
}

bool RgbDevice::supportsZoneEffect(int zoneIndex, int effectType) const
{
    return zoneIndex >= 0 && zoneIndex < m_zones.size() && supportsEffect(effectType);
}

bool RgbDevice::supportsZoneEffectSpeed(int zoneIndex, int effectType) const
{
    return zoneIndex >= 0 && zoneIndex < m_zones.size() && supportsEffectSpeed(effectType);
}

bool RgbDevice::supportsZoneEffectBrightness(int zoneIndex, int effectType) const
{
    return zoneIndex >= 0 && zoneIndex < m_zones.size() && supportsEffectBrightness(effectType);
}

} // namespace lumacore
