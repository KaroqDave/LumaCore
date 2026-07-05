// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QTime>
#include <QTimer>

namespace lumacore {

class AppController;
class SettingsController;

class ProfileScheduleRunner final : public QObject
{
    Q_OBJECT

public:
    explicit ProfileScheduleRunner(
        SettingsController* settingsController,
        AppController* appController,
        QObject* parent = nullptr
    );

    [[nodiscard]] static QTime parseScheduledTime(const QString& value);
    [[nodiscard]] static qint64 millisecondsUntilNextRun(
        const QDateTime& now,
        const QTime& scheduledTime
    );

    [[nodiscard]] bool suspended() const;

public slots:
    void evaluateNow();
    // Stand the local runner down while the daemon owns the schedule; resuming
    // re-arms without retro-firing a boundary that passed while suspended.
    void setSuspended(bool suspended);

private:
    void resetAndSchedule();
    void scheduleNextCheck();

    SettingsController* m_settingsController {nullptr};
    AppController* m_appController {nullptr};
    QTimer m_timer;
    QDate m_lastAttemptDate;
    bool m_skipMissedRun {true};
    bool m_suspended {false};
};

} // namespace lumacore
