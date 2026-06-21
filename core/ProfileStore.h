#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace lumacore {

class ProfileStore
{
public:
    explicit ProfileStore(QString directoryPath = {});

    [[nodiscard]] QString directoryPath() const;
    [[nodiscard]] QString filePath(const QString& profileName) const;
    [[nodiscard]] QStringList names() const;

    [[nodiscard]] bool save(
        const QString& profileName,
        const QJsonObject& profile,
        QString* errorMessage = nullptr
    ) const;
    [[nodiscard]] bool load(
        const QString& profileName,
        QJsonObject* profile,
        QString* errorMessage = nullptr
    ) const;
    [[nodiscard]] bool remove(const QString& profileName, QString* errorMessage = nullptr) const;
    [[nodiscard]] bool rename(
        const QString& oldProfileName,
        const QString& newProfileName,
        QString* errorMessage = nullptr
    ) const;
    [[nodiscard]] bool importFile(
        const QString& sourcePath,
        QString* importedProfileName = nullptr,
        QString* errorMessage = nullptr
    ) const;
    [[nodiscard]] bool exportFile(
        const QString& profileName,
        const QString& destinationPath,
        QString* errorMessage = nullptr
    ) const;

    [[nodiscard]] static QString normalizeName(const QString& profileName);

private:
    void migrateLegacyProfilesIfNeeded();

    QString m_directoryPath;
};

} // namespace lumacore
