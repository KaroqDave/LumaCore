#include "ui/DeviceGroupStore.h"

#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace lumacore {

QString DeviceGroupStore::settingsKey(const QString& groupName)
{
    QString key = groupName.trimmed();
    key.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    return key.isEmpty() ? QString() : key;
}

QStringList DeviceGroupStore::uniqueNonEmptyStrings(const QStringList& values)
{
    QStringList result;
    for (const QString& value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty() && !result.contains(normalized)) {
            result.append(normalized);
        }
    }
    return result;
}

QStringList DeviceGroupStore::names() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("deviceGroups"));
    QStringList values = settings.value(QStringLiteral("names")).toStringList();
    settings.endGroup();
    values.removeAll(QString());
    values.removeDuplicates();
    values.sort(Qt::CaseInsensitive);
    return values;
}

QStringList DeviceGroupStore::deviceIds(const QString& groupName) const
{
    const QString key = settingsKey(groupName);
    if (groupName.isEmpty() || key.isEmpty()) {
        return {};
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("deviceGroups"));
    const QStringList storedNames = settings.value(QStringLiteral("names")).toStringList();
    const auto existingName = std::find_if(storedNames.cbegin(), storedNames.cend(), [&groupName](const QString& name) {
        return name.compare(groupName, Qt::CaseInsensitive) == 0;
    });
    if (existingName == storedNames.cend()) {
        settings.endGroup();
        return {};
    }

    settings.beginGroup(settingsKey(*existingName));
    const QStringList ids = uniqueNonEmptyStrings(settings.value(QStringLiteral("deviceIds")).toStringList());
    settings.endGroup();
    settings.endGroup();
    return ids;
}

bool DeviceGroupStore::save(const QString& groupName, const QStringList& deviceIds)
{
    const QString key = settingsKey(groupName);
    if (groupName.isEmpty() || key.isEmpty() || deviceIds.isEmpty()) {
        return false;
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("deviceGroups"));
    QStringList storedNames = settings.value(QStringLiteral("names")).toStringList();
    const auto existingName = std::find_if(storedNames.begin(), storedNames.end(), [&groupName](const QString& name) {
        return name.compare(groupName, Qt::CaseInsensitive) == 0;
    });
    if (existingName == storedNames.end()) {
        storedNames.append(groupName);
    } else {
        *existingName = groupName;
    }
    storedNames = uniqueNonEmptyStrings(storedNames);
    storedNames.sort(Qt::CaseInsensitive);
    settings.setValue(QStringLiteral("names"), storedNames);
    settings.beginGroup(key);
    settings.setValue(QStringLiteral("name"), groupName);
    settings.setValue(QStringLiteral("deviceIds"), deviceIds);
    settings.endGroup();
    settings.endGroup();
    return true;
}

bool DeviceGroupStore::remove(const QString& groupName)
{
    const QString key = settingsKey(groupName);
    if (groupName.isEmpty() || key.isEmpty()) {
        return false;
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("deviceGroups"));
    QStringList storedNames = settings.value(QStringLiteral("names")).toStringList();
    const int before = storedNames.size();
    storedNames.erase(
        std::remove_if(storedNames.begin(), storedNames.end(), [&groupName](const QString& name) {
            return name.compare(groupName, Qt::CaseInsensitive) == 0;
        }),
        storedNames.end()
    );
    settings.setValue(QStringLiteral("names"), storedNames);
    settings.remove(key);
    settings.endGroup();
    return storedNames.size() != before;
}

} // namespace lumacore
