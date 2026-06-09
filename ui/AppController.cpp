#include "ui/AppController.h"

#include "core/RgbColor.h"

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
    if (m_deviceManager == nullptr || !color.isValid()) {
        setStatusMessage(QStringLiteral("Could not apply color."));
        return false;
    }

    const bool changed = m_deviceManager->setZoneStaticColor(deviceIndex, zoneIndex, RgbColor::fromQColor(color));
    if (!changed) {
        setStatusMessage(QStringLiteral("Could not apply color to selected zone."));
    }

    return changed;
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
