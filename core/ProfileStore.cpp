#include "core/ProfileStore.h"

#include "core/PortablePaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>

#include <utility>

namespace lumacore {

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

ProfileStore::ProfileStore(QString directoryPath)
    : m_directoryPath(std::move(directoryPath))
{
    if (m_directoryPath.trimmed().isEmpty()) {
        m_directoryPath = portableProfilesDirectory();
    }

    m_directoryPath = QDir::cleanPath(QFileInfo(m_directoryPath).absoluteFilePath());
    migrateLegacyProfilesIfNeeded();
}

QString ProfileStore::directoryPath() const
{
    return m_directoryPath;
}

QString ProfileStore::filePath(const QString& profileName) const
{
    return QDir(directoryPath()).filePath(normalizeName(profileName) + QStringLiteral(".json"));
}

QStringList ProfileStore::names() const
{
    const QDir profilesDir(directoryPath());
    QStringList profileNames;

    for (const QFileInfo& fileInfo : profilesDir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        profileNames.append(fileInfo.completeBaseName());
    }

    return profileNames;
}

bool ProfileStore::save(
    const QString& profileName,
    const QJsonObject& profile,
    QString* errorMessage
) const
{
    QDir profilesDir(directoryPath());
    if (!profilesDir.exists() && !profilesDir.mkpath(QStringLiteral("."))) {
        setError(
            errorMessage,
            QStringLiteral("Could not create profiles directory: %1").arg(profilesDir.absolutePath())
        );
        return false;
    }

    QSaveFile file(filePath(profileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(errorMessage, QStringLiteral("Could not write profile: %1").arg(file.errorString()));
        return false;
    }

    file.write(QJsonDocument(profile).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(errorMessage, QStringLiteral("Could not commit profile: %1").arg(file.errorString()));
        return false;
    }

    return true;
}

bool ProfileStore::load(
    const QString& profileName,
    QJsonObject* profile,
    QString* errorMessage
) const
{
    QFile file(filePath(profileName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(errorMessage, QStringLiteral("Could not open profile: %1").arg(file.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("Invalid profile JSON: %1").arg(parseError.errorString()));
        return false;
    }

    if (profile != nullptr) {
        *profile = document.object();
    }
    return true;
}

bool ProfileStore::remove(const QString& profileName, QString* errorMessage) const
{
    const QString normalizedName = normalizeName(profileName);
    QFile file(filePath(normalizedName));
    if (!file.exists()) {
        setError(errorMessage, QStringLiteral("Profile '%1' does not exist.").arg(normalizedName));
        return false;
    }

    if (!file.remove()) {
        setError(
            errorMessage,
            QStringLiteral("Could not delete profile '%1': %2").arg(normalizedName, file.errorString())
        );
        return false;
    }

    return true;
}

bool ProfileStore::rename(
    const QString& oldProfileName,
    const QString& newProfileName,
    QString* errorMessage
) const
{
    const QString normalizedOldName = normalizeName(oldProfileName);
    const QString normalizedNewName = normalizeName(newProfileName);

    if (normalizedOldName == normalizedNewName) {
        setError(errorMessage, QStringLiteral("Profile already uses that name."));
        return false;
    }

    QFile oldFile(filePath(normalizedOldName));
    if (!oldFile.exists()) {
        setError(errorMessage, QStringLiteral("Profile '%1' does not exist.").arg(normalizedOldName));
        return false;
    }

    if (QFile::exists(filePath(normalizedNewName))) {
        setError(errorMessage, QStringLiteral("Profile '%1' already exists.").arg(normalizedNewName));
        return false;
    }

    if (!oldFile.rename(filePath(normalizedNewName))) {
        setError(errorMessage, QStringLiteral("Could not rename profile: %1").arg(oldFile.errorString()));
        return false;
    }

    return true;
}

bool ProfileStore::importFile(
    const QString& sourcePath,
    QString* importedProfileName,
    QString* errorMessage
) const
{
    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(errorMessage, QStringLiteral("Could not open profile import: %1").arg(sourceFile.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(sourceFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("Invalid profile JSON: %1").arg(parseError.errorString()));
        return false;
    }

    QJsonObject profile = document.object();
    if (!profile.value(QStringLiteral("devices")).isArray()) {
        setError(errorMessage, QStringLiteral("Invalid LumaCore profile: missing devices array."));
        return false;
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString requestedName = profile.value(QStringLiteral("profileName")).toString(sourceInfo.completeBaseName());
    const QString normalizedName = normalizeName(requestedName);
    if (QFile::exists(filePath(normalizedName))) {
        setError(errorMessage, QStringLiteral("Profile '%1' already exists.").arg(normalizedName));
        return false;
    }

    profile.insert(QStringLiteral("profileName"), normalizedName);
    if (!save(normalizedName, profile, errorMessage)) {
        return false;
    }

    if (importedProfileName != nullptr) {
        *importedProfileName = normalizedName;
    }
    return true;
}

bool ProfileStore::exportFile(
    const QString& profileName,
    const QString& destinationPath,
    QString* errorMessage
) const
{
    QJsonObject profile;
    if (!load(profileName, &profile, errorMessage)) {
        return false;
    }

    QSaveFile destinationFile(destinationPath);
    if (!destinationFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(errorMessage, QStringLiteral("Could not export profile: %1").arg(destinationFile.errorString()));
        return false;
    }

    destinationFile.write(QJsonDocument(profile).toJson(QJsonDocument::Indented));
    if (!destinationFile.commit()) {
        setError(errorMessage, QStringLiteral("Could not commit profile export: %1").arg(destinationFile.errorString()));
        return false;
    }
    return true;
}

QString ProfileStore::normalizeName(const QString& profileName)
{
    QString normalized = profileName.trimmed();
    if (normalized.isEmpty()) {
        normalized = QStringLiteral("default");
    }

    normalized.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    return normalized;
}

void ProfileStore::migrateLegacyProfilesIfNeeded()
{
    const QDir destinationDirectory(m_directoryPath);
    const QDir legacyDirectory(QDir::current().filePath(QStringLiteral("profiles")));
    if (!legacyDirectory.exists()
        || QDir::cleanPath(legacyDirectory.absolutePath()) == QDir::cleanPath(destinationDirectory.absolutePath())) {
        return;
    }

    const QFileInfoList legacyProfiles =
        legacyDirectory.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (legacyProfiles.isEmpty()) {
        return;
    }

    if (!QDir().mkpath(m_directoryPath)) {
        return;
    }

    for (const QFileInfo& legacyProfile : legacyProfiles) {
        const QString destinationPath = destinationDirectory.filePath(legacyProfile.fileName());
        if (!QFile::exists(destinationPath)) {
            QFile::copy(legacyProfile.absoluteFilePath(), destinationPath);
        }
    }
}

} // namespace lumacore
