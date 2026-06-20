#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace lumacore {

class ProfileStore
{
public:
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

    [[nodiscard]] static QString normalizeName(const QString& profileName);
};

} // namespace lumacore
