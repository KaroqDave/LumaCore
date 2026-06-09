#include "core/DeviceManager.h"

#include "backends/mock/MockBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace lumacore {

DeviceManager::DeviceManager(QObject* parent)
    : QObject(parent)
{
}

void DeviceManager::initializeMockDevices()
{
    m_devices.clear();

    const MockBackend mockBackend;
    for (std::unique_ptr<RgbDevice>& device : mockBackend.createDevices()) {
        registerDevice(std::move(device));
    }

    emit devicesChanged();
    emit logMessage(QStringLiteral("Loaded %1 mock device(s).").arg(deviceCount()));
}

int DeviceManager::deviceCount() const
{
    return static_cast<int>(m_devices.size());
}

RgbDevice* DeviceManager::deviceAt(int index)
{
    if (index < 0 || index >= deviceCount()) {
        return nullptr;
    }

    return m_devices.at(static_cast<std::size_t>(index)).get();
}

const RgbDevice* DeviceManager::deviceAt(int index) const
{
    if (index < 0 || index >= deviceCount()) {
        return nullptr;
    }

    return m_devices.at(static_cast<std::size_t>(index)).get();
}

const std::vector<std::unique_ptr<RgbDevice>>& DeviceManager::devices() const
{
    return m_devices;
}

bool DeviceManager::setZoneStaticColor(int deviceIndex, int zoneIndex, const RgbColor& color)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr) {
        return false;
    }

    const bool changed = device->setZoneStaticColor(zoneIndex, color);
    if (changed) {
        const RgbZone& zone = device->zones().at(zoneIndex);
        emit logMessage(QStringLiteral("%1 / %2 set to %3").arg(device->name(), zone.name(), color.toHexString()));
    }

    return changed;
}

bool DeviceManager::saveProfile(const QString& profileName, QString* errorMessage)
{
    const QString normalizedName = normalizeProfileName(profileName);
    QDir profilesDir(profilesDirectoryPath());
    if (!profilesDir.exists() && !profilesDir.mkpath(QStringLiteral("."))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create profiles directory: %1").arg(profilesDir.absolutePath());
        }
        return false;
    }

    QJsonArray devicesJson;
    for (const std::unique_ptr<RgbDevice>& device : m_devices) {
        QJsonArray zonesJson;
        for (const RgbZone& zone : device->zones()) {
            zonesJson.append(QJsonObject {
                {QStringLiteral("name"), zone.name()},
                {QStringLiteral("type"), zone.typeName()},
                {QStringLiteral("ledCount"), zone.ledCount()},
                {QStringLiteral("color"), zone.currentColor().toHexString()},
                {QStringLiteral("rgb"), zone.currentColor().toJson()},
            });
        }

        devicesJson.append(QJsonObject {
            {QStringLiteral("id"), device->id()},
            {QStringLiteral("name"), device->name()},
            {QStringLiteral("vendor"), device->vendor()},
            {QStringLiteral("type"), device->typeName()},
            {QStringLiteral("zones"), zonesJson},
        });
    }

    const QJsonObject root {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("application"), QStringLiteral("LumaCore")},
        {QStringLiteral("profileName"), normalizedName},
        {QStringLiteral("devices"), devicesJson},
    };

    QSaveFile file(profileFilePath(normalizedName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not write profile: %1").arg(file.errorString());
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not commit profile: %1").arg(file.errorString());
        }
        return false;
    }

    emit logMessage(QStringLiteral("Saved profile '%1'.").arg(normalizedName));
    return true;
}

bool DeviceManager::loadProfile(const QString& profileName, QString* errorMessage)
{
    const QString normalizedName = normalizeProfileName(profileName);
    QFile file(profileFilePath(normalizedName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not open profile: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid profile JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    int appliedZones = 0;
    const QJsonArray devicesJson = document.object().value(QStringLiteral("devices")).toArray();
    for (const QJsonValue& deviceValue : devicesJson) {
        const QJsonObject deviceObject = deviceValue.toObject();
        const QString deviceId = deviceObject.value(QStringLiteral("id")).toString();

        for (int deviceIndex = 0; deviceIndex < deviceCount(); ++deviceIndex) {
            RgbDevice* device = deviceAt(deviceIndex);
            if (device == nullptr || device->id() != deviceId) {
                continue;
            }

            const QJsonArray zonesJson = deviceObject.value(QStringLiteral("zones")).toArray();
            for (const QJsonValue& zoneValue : zonesJson) {
                const QJsonObject zoneObject = zoneValue.toObject();
                const QString zoneName = zoneObject.value(QStringLiteral("name")).toString();
                bool colorOk = false;
                const RgbColor color = RgbColor::fromHexString(zoneObject.value(QStringLiteral("color")).toString(), &colorOk);
                if (!colorOk) {
                    emit logMessage(QStringLiteral("Skipped zone '%1' with invalid color.").arg(zoneName));
                    continue;
                }

                for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
                    if (device->zones().at(zoneIndex).name() != zoneName) {
                        continue;
                    }

                    if (setZoneStaticColor(deviceIndex, zoneIndex, color)) {
                        ++appliedZones;
                    }
                    break;
                }
            }
        }
    }

    if (appliedZones == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Profile did not match any available mock zones.");
        }
        return false;
    }

    emit logMessage(QStringLiteral("Loaded profile '%1' and applied %2 zone(s).").arg(normalizedName).arg(appliedZones));
    return true;
}

QStringList DeviceManager::profileNames() const
{
    const QDir profilesDir(profilesDirectoryPath());
    QStringList names;

    for (const QFileInfo& fileInfo : profilesDir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        names.append(fileInfo.completeBaseName());
    }

    return names;
}

QString DeviceManager::profilesDirectoryPath() const
{
    return QDir::current().filePath(QStringLiteral("profiles"));
}

void DeviceManager::registerDevice(std::unique_ptr<RgbDevice> device)
{
    if (!device) {
        return;
    }

    const int deviceIndex = deviceCount();
    connect(device.get(), &RgbDevice::zoneChanged, this, [this, deviceIndex](int zoneIndex) {
        emit zoneColorChanged(deviceIndex, zoneIndex);
    });

    m_devices.push_back(std::move(device));
}

QString DeviceManager::profileFilePath(const QString& profileName) const
{
    return QDir(profilesDirectoryPath()).filePath(normalizeProfileName(profileName) + QStringLiteral(".json"));
}

QString DeviceManager::normalizeProfileName(const QString& profileName)
{
    QString normalized = profileName.trimmed();
    if (normalized.isEmpty()) {
        normalized = QStringLiteral("default");
    }

    normalized.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    return normalized;
}

} // namespace lumacore
