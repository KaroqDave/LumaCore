// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/ScheduleTime.h"

#include <QTimeZone>

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

qint64 millisecondsUntilNextScheduleRun(const QDateTime& now, const QTime& scheduledTime)
{
    if (!now.isValid() || !scheduledTime.isValid()) {
        return 0;
    }

    QDateTime nextRun(now.date(), scheduledTime, now.timeZone());
    if (nextRun <= now) {
        nextRun = nextRun.addDays(1);
    }
    return now.msecsTo(nextRun);
}

} // namespace lumacore
