// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace lumacore {

class DeviceGroupStore
{
public:
    [[nodiscard]] QStringList names() const;
    [[nodiscard]] QStringList deviceIds(const QString& groupName) const;

    bool save(const QString& groupName, const QStringList& deviceIds);
    bool remove(const QString& groupName);

private:
    [[nodiscard]] static QString settingsKey(const QString& groupName);
    [[nodiscard]] static QStringList uniqueNonEmptyStrings(const QStringList& values);
};

} // namespace lumacore
