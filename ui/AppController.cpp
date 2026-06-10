#include "ui/AppController.h"

#include "core/RgbColor.h"
#include "core/RgbEffect.h"

#include <QDateTime>

namespace lumacore {

AppController::AppController(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
{
    setStatusMessage(QStringLiteral("Ready. Mock backend only."));
    appendLog(QStringLiteral("Application started in mock-only mode."));

    if (m_deviceManager == nullptr) {
        return;
    }

    connect(m_deviceManager, &DeviceManager::logMessage, this, [this](const QString& message) {
        appendLog(message);
        setStatusMessage(message);
    });

    connect(m_deviceManager, &DeviceManager::zoneChanged, this, [this](int deviceIndex, int zoneIndex) {
        emit zoneDataChanged(deviceIndex, zoneIndex);
    });
}

QString AppController::statusMessage() const
{
    return m_statusMessage;
}

QString AppController::logText() const
{
    return m_logLines.join(QLatin1Char('\n'));
}

QString AppController::profilesDirectory() const
{
    return m_deviceManager == nullptr ? QString() : m_deviceManager->profilesDirectoryPath();
}

bool AppController::applyStaticColor(int deviceIndex, int zoneIndex, const QColor& color)
{
    return applyEffect(deviceIndex, zoneIndex, static_cast<int>(RgbEffectType::Static), color, 1.0, 100);
}

bool AppController::applyEffect(int deviceIndex, int zoneIndex, int effectType, const QColor& color, double speed, int brightness)
{
    if (m_deviceManager == nullptr || !color.isValid()) {
        setStatusMessage(QStringLiteral("Could not apply effect."));
        return false;
    }

    const RgbEffect effect(
        static_cast<RgbEffectType>(effectType),
        RgbColor::fromQColor(color),
        speed,
        brightness
    );

    const bool changed = m_deviceManager->applyZoneEffect(deviceIndex, zoneIndex, effect);
    if (!changed) {
        setStatusMessage(QStringLiteral("Could not apply effect to selected zone."));
    }

    return changed;
}

int AppController::zoneEffectType(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return static_cast<int>(RgbEffectType::Static);
    }

    return static_cast<int>(device->zones().at(zoneIndex).effect().type());
}

QColor AppController::zoneEffectColor(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).effect().color().toQColor();
}

double AppController::zoneEffectSpeed(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return 1.0;
    }

    return device->zones().at(zoneIndex).effect().speed();
}

int AppController::zoneEffectBrightness(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return 100;
    }

    return device->zones().at(zoneIndex).effect().brightness();
}

bool AppController::updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool updated = m_deviceManager->updateZone(deviceIndex, zoneIndex, name, ledCount, &errorMessage);
    if (!updated) {
        appendLog(errorMessage);
        setStatusMessage(errorMessage);
    }

    return updated;
}

int AppController::zoneCount(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device == nullptr ? 0 : static_cast<int>(device->zones().size());
}

QString AppController::deviceName(int deviceIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    return device == nullptr ? QString() : device->name();
}

QString AppController::zoneName(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).name();
}

QString AppController::zoneTypeName(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).typeName();
}

int AppController::zoneLedCount(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return 0;
    }

    return device->zones().at(zoneIndex).ledCount();
}

QColor AppController::zoneColor(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).currentColor().toQColor();
}

QString AppController::zoneColorHex(int deviceIndex, int zoneIndex) const
{
    const RgbDevice* device = m_deviceManager == nullptr ? nullptr : m_deviceManager->deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        return {};
    }

    return device->zones().at(zoneIndex).currentColor().toHexString();
}

bool AppController::saveProfile(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool saved = m_deviceManager->saveProfile(profileName, &errorMessage);
    if (!saved) {
        appendLog(errorMessage);
        setStatusMessage(errorMessage);
    } else {
        emit profilesChanged();
    }

    return saved;
}

bool AppController::loadProfile(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool loaded = m_deviceManager->loadProfile(profileName, &errorMessage);
    if (!loaded) {
        appendLog(errorMessage);
        setStatusMessage(errorMessage);
    }

    return loaded;
}

bool AppController::deleteProfile(const QString& profileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool deleted = m_deviceManager->deleteProfile(profileName, &errorMessage);
    if (!deleted) {
        appendLog(errorMessage);
        setStatusMessage(errorMessage);
    } else {
        emit profilesChanged();
    }

    return deleted;
}

bool AppController::renameProfile(const QString& oldProfileName, const QString& newProfileName)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    QString errorMessage;
    const bool renamed = m_deviceManager->renameProfile(oldProfileName, newProfileName, &errorMessage);
    if (!renamed) {
        appendLog(errorMessage);
        setStatusMessage(errorMessage);
    } else {
        emit profilesChanged();
    }

    return renamed;
}

QStringList AppController::profileNames() const
{
    return m_deviceManager == nullptr ? QStringList {} : m_deviceManager->profileNames();
}

void AppController::appendLog(const QString& message)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    m_logLines.append(QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
    emit logTextChanged();
}

void AppController::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message) {
        return;
    }

    m_statusMessage = message;
    emit statusMessageChanged();
}

} // namespace lumacore
