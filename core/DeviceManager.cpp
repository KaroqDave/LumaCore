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

bool DeviceManager::updateZone(int deviceIndex, int zoneIndex, const QString& name, int ledCount, QString* errorMessage)
{
    RgbDevice* device = deviceAt(deviceIndex);
    if (device == nullptr || zoneIndex < 0 || zoneIndex >= device->zones().size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not update selected zone.");
        }
        return false;
    }

    const QString sanitizedName = name.trimmed();
    if (sanitizedName.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Zone name cannot be empty.");
        }
        return false;
    }

    if (ledCount < 1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("LED count must be at least 1.");
        }
        return false;
    }

    const QString previousName = device->zones().at(zoneIndex).name();
    const int previousLedCount = device->zones().at(zoneIndex).ledCount();
    const bool nameChanged = device->setZoneName(zoneIndex, sanitizedName);
    const bool ledCountChanged = device->setZoneLedCount(zoneIndex, ledCount);

    if (!nameChanged && !ledCountChanged) {
        emit logMessage(QStringLiteral("%1 / %2 is already up to date.").arg(device->name(), sanitizedName));
        return true;
    }

    emit logMessage(QStringLiteral("%1 / %2 updated (%3 LEDs).")
                        .arg(device->name(), sanitizedName)
                        .arg(ledCount));
    if (nameChanged && previousName != sanitizedName) {
        emit logMessage(QStringLiteral("Renamed zone '%1' to '%2'.").arg(previousName, sanitizedName));
    }
    if (ledCountChanged && previousLedCount != ledCount) {
        emit logMessage(QStringLiteral("Changed '%1' from %2 to %3 LED(s).")
                            .arg(sanitizedName)
                            .arg(previousLedCount)
                            .arg(ledCount));
    }

    return true;
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
        int zoneIndex = 0;
        for (const RgbZone& zone : device->zones()) {
            zonesJson.append(QJsonObject {
                {QStringLiteral("index"), zoneIndex},
                {QStringLiteral("name"), zone.name()},
                {QStringLiteral("type"), zone.typeName()},
                {QStringLiteral("ledCount"), zone.ledCount()},
                {QStringLiteral("color"), zone.currentColor().toHexString()},
                {QStringLiteral("rgb"), zone.currentColor().toJson()},
            });
            ++zoneIndex;
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

                int matchedZoneIndex = -1;
                for (int zoneIndex = 0; zoneIndex < device->zones().size(); ++zoneIndex) {
                    if (device->zones().at(zoneIndex).name() != zoneName) {
                        continue;
                    }

                    matchedZoneIndex = zoneIndex;
                    break;
                }

                if (matchedZoneIndex < 0) {
                    const int storedZoneIndex = zoneObject.value(QStringLiteral("index")).toInt(-1);
                    if (storedZoneIndex >= 0 && storedZoneIndex < device->zones().size()) {
                        matchedZoneIndex = storedZoneIndex;
                    }
                }

                if (matchedZoneIndex < 0) {
                    continue;
                }

                const int ledCount = zoneObject.value(QStringLiteral("ledCount")).toInt(device->zones().at(matchedZoneIndex).ledCount());
                const bool renamed = device->setZoneName(matchedZoneIndex, zoneName);
                const bool resized = device->setZoneLedCount(matchedZoneIndex, qMax(1, ledCount));
                Q_UNUSED(renamed)
                Q_UNUSED(resized)

                if (setZoneStaticColor(deviceIndex, matchedZoneIndex, color)) {
                    ++appliedZones;
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

bool DeviceManager::deleteProfile(const QString& profileName, QString* errorMessage)
{
    const QString normalizedName = normalizeProfileName(profileName);
    QFile file(profileFilePath(normalizedName));
    if (!file.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Profile '%1' does not exist.").arg(normalizedName);
        }
        return false;
    }

    if (!file.remove()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not delete profile '%1': %2").arg(normalizedName, file.errorString());
        }
        return false;
    }

    emit logMessage(QStringLiteral("Deleted profile '%1'.").arg(normalizedName));
    return true;
}

bool DeviceManager::renameProfile(const QString& oldProfileName, const QString& newProfileName, QString* errorMessage)
{
    const QString normalizedOldName = normalizeProfileName(oldProfileName);
    const QString normalizedNewName = normalizeProfileName(newProfileName);

    if (normalizedOldName == normalizedNewName) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Profile already uses that name.");
        }
        return false;
    }

    QFile oldFile(profileFilePath(normalizedOldName));
    if (!oldFile.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Profile '%1' does not exist.").arg(normalizedOldName);
        }
        return false;
    }

    QFile newFile(profileFilePath(normalizedNewName));
    if (newFile.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Profile '%1' already exists.").arg(normalizedNewName);
        }
        return false;
    }

    if (!oldFile.rename(profileFilePath(normalizedNewName))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not rename profile: %1").arg(oldFile.errorString());
        }
        return false;
    }

    emit logMessage(QStringLiteral("Renamed profile '%1' to '%2'.").arg(normalizedOldName, normalizedNewName));
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
        emit zoneChanged(deviceIndex, zoneIndex);
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
