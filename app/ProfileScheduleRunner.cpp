// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/ProfileScheduleRunner.h"

#include "core/ScheduleTime.h"
#include "ui/AppController.h"
#include "ui/SettingsController.h"

#include <QTimer>
#include <QTimeZone>

#include <algorithm>

namespace lumacore {

namespace {

constexpr int minimumTimerIntervalMs = 1000;
constexpr int maximumTimerIntervalMs = 60 * 1000;

} // namespace

ProfileScheduleRunner::ProfileScheduleRunner(
    SettingsController* settingsController,
    AppController* appController,
    QObject* parent
)
    : QObject(parent)
    , m_settingsController(settingsController)
    , m_appController(appController)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ProfileScheduleRunner::evaluateNow);

    if (m_settingsController != nullptr) {
        connect(
            m_settingsController,
            &SettingsController::scheduledProfileEnabledChanged,
            this,
            &ProfileScheduleRunner::resetAndSchedule
        );
        connect(
            m_settingsController,
            &SettingsController::scheduledProfileChanged,
            this,
            &ProfileScheduleRunner::resetAndSchedule
        );
        connect(
            m_settingsController,
            &SettingsController::scheduledProfileTimeChanged,
            this,
            &ProfileScheduleRunner::resetAndSchedule
        );
    }

    QTimer::singleShot(0, this, &ProfileScheduleRunner::evaluateNow);
}

QTime ProfileScheduleRunner::parseScheduledTime(const QString& value)
{
    return parseScheduleTime(value);
}

qint64 ProfileScheduleRunner::millisecondsUntilNextRun(
    const QDateTime& now,
    const QTime& scheduledTime
)
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

void ProfileScheduleRunner::evaluateNow()
{
    if (m_settingsController == nullptr || m_appController == nullptr) {
        return;
    }

    if (!m_settingsController->scheduledProfileEnabled()
        || m_settingsController->scheduledProfile().isEmpty()) {
        m_timer.stop();
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QTime scheduledTime = parseScheduledTime(m_settingsController->scheduledProfileTime());
    if (now.time() >= scheduledTime && m_lastAttemptDate != now.date()) {
        m_lastAttemptDate = now.date();
        if (!m_skipMissedRun) {
            const bool applied =
                m_appController->applyScheduledProfile(m_settingsController->scheduledProfile());
            Q_UNUSED(applied)
        }
    }
    m_skipMissedRun = false;

    scheduleNextCheck();
}

void ProfileScheduleRunner::resetAndSchedule()
{
    m_lastAttemptDate = {};
    m_skipMissedRun = true;
    QTimer::singleShot(0, this, &ProfileScheduleRunner::evaluateNow);
}

void ProfileScheduleRunner::scheduleNextCheck()
{
    if (m_settingsController == nullptr
        || !m_settingsController->scheduledProfileEnabled()
        || m_settingsController->scheduledProfile().isEmpty()) {
        m_timer.stop();
        return;
    }

    const qint64 untilNextRun = millisecondsUntilNextRun(
        QDateTime::currentDateTime(),
        parseScheduledTime(m_settingsController->scheduledProfileTime())
    );
    const int interval = static_cast<int>(
        std::clamp<qint64>(
            untilNextRun,
            minimumTimerIntervalMs,
            maximumTimerIntervalMs
        )
    );
    m_timer.start(interval);
}

} // namespace lumacore
