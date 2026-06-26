// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/ScheduleTime.h"

namespace lumacore {

namespace {

constexpr int kDefaultScheduledHour = 18;
constexpr int kDefaultScheduledMinute = 0;

QTime defaultScheduleTime()
{
    return QTime(kDefaultScheduledHour, kDefaultScheduledMinute);
}

} // namespace

QTime parseScheduleTime(const QString& value)
{
    const QTime parsed = QTime::fromString(value.trimmed(), QStringLiteral("HH:mm"));
    return parsed.isValid() ? parsed : defaultScheduleTime();
}

QString normalizeScheduleTime(const QString& value)
{
    return parseScheduleTime(value).toString(QStringLiteral("HH:mm"));
}

} // namespace lumacore
