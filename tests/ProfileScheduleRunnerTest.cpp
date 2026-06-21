#include "app/ProfileScheduleRunner.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QTime>
#include <QTimeZone>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    using lumacore::ProfileScheduleRunner;

    if (!require(
            ProfileScheduleRunner::parseScheduledTime(QStringLiteral("07:30")) == QTime(7, 30),
            "valid scheduled times should parse"
        )
        || !require(
            ProfileScheduleRunner::parseScheduledTime(QStringLiteral("invalid")) == QTime(18, 0),
            "invalid scheduled times should fall back to 18:00"
        )) {
        return 1;
    }

    const QDate date(2026, 6, 21);
    const QTimeZone utc = QTimeZone::utc();
    const QDateTime beforeRun(date, QTime(7, 0), utc);
    const QDateTime afterRun(date, QTime(8, 0), utc);

    if (!require(
            ProfileScheduleRunner::millisecondsUntilNextRun(beforeRun, QTime(7, 30))
                == 30 * 60 * 1000,
            "next run should be later today when the scheduled time is ahead"
        )
        || !require(
            ProfileScheduleRunner::millisecondsUntilNextRun(afterRun, QTime(7, 30))
                == 23 * 60 * 60 * 1000 + 30 * 60 * 1000,
            "next run should roll to tomorrow after today's scheduled time has passed"
        )
        || !require(
            ProfileScheduleRunner::millisecondsUntilNextRun(QDateTime(), QTime(7, 30)) == 0,
            "invalid current times should not schedule a delayed run"
        )
        || !require(
            ProfileScheduleRunner::millisecondsUntilNextRun(beforeRun, QTime()) == 0,
            "invalid scheduled times should not schedule a delayed run"
        )) {
        return 1;
    }

    return 0;
}
