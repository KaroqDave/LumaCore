// SPDX-License-Identifier: GPL-2.0-or-later

#include "daemon/ScheduleService.h"

#include "core/DeviceManager.h"
#include "core/ScheduleTime.h"

#include <algorithm>

namespace lumacore {

namespace {

constexpr int minimumTimerIntervalMs = 1000;
constexpr int maximumTimerIntervalMs = 60 * 1000;

} // namespace

ScheduleService::ScheduleService(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ScheduleService::evaluateNow);

    loadPersistedConfig();
    QTimer::singleShot(0, this, &ScheduleService::evaluateNow);
}

ScheduleService::Config ScheduleService::config() const
{
    return m_config;
}

ScheduleService::Config ScheduleService::setConfig(const Config& config)
{
    m_config = sanitizedConfig(config);
    persistConfig();
    m_lastAttemptDate = {};
    m_skipMissedRun = true;
    QTimer::singleShot(0, this, &ScheduleService::evaluateNow);
    return m_config;
}

void ScheduleService::setClock(std::function<QDateTime()> clock)
{
    m_clock = std::move(clock);
}

void ScheduleService::evaluateNow()
{
    if (m_deviceManager == nullptr) {
        return;
    }

    if (!m_config.enabled || m_config.profileName.isEmpty()) {
        m_timer.stop();
        return;
    }

    const QDateTime now = currentDateTime();
    const QTime scheduledTime = parseScheduleTime(m_config.time);
    if (now.time() >= scheduledTime && m_lastAttemptDate != now.date()) {
        m_lastAttemptDate = now.date();
        if (!m_skipMissedRun) {
            applyScheduledProfile();
        }
    }
    m_skipMissedRun = false;

    scheduleNextCheck();
}

ScheduleService::Config ScheduleService::sanitizedConfig(Config config)
{
    config.profileName = config.profileName.trimmed();
    config.enabled = config.enabled && !config.profileName.isEmpty();
    config.time = normalizeScheduleTime(config.time);
    return config;
}

QDateTime ScheduleService::currentDateTime() const
{
    return m_clock ? m_clock() : QDateTime::currentDateTime();
}

void ScheduleService::loadPersistedConfig()
{
    Config config;
    config.profileName = m_settings.value(QStringLiteral("schedule/profile")).toString();
    config.enabled = m_settings.value(QStringLiteral("schedule/enabled"), false).toBool();
    config.time = m_settings.value(QStringLiteral("schedule/time")).toString();
    m_config = sanitizedConfig(config);
}

void ScheduleService::persistConfig()
{
    if (m_config.profileName.isEmpty()) {
        m_settings.remove(QStringLiteral("schedule/profile"));
    } else {
        m_settings.setValue(QStringLiteral("schedule/profile"), m_config.profileName);
    }
    m_settings.setValue(QStringLiteral("schedule/enabled"), m_config.enabled);
    m_settings.setValue(QStringLiteral("schedule/time"), m_config.time);
}

void ScheduleService::applyScheduledProfile()
{
    m_deviceManager->activityLog().info(
        LogCategory::Profile,
        QStringLiteral("Scheduled profile '%1' fired (daemon schedule).").arg(m_config.profileName)
    );

    const QVariantMap report = m_deviceManager->applyProfileWithReport(m_config.profileName);
    const int failedZones = report.value(QStringLiteral("failedZones")).toInt();
    if (failedZones > 0) {
        m_deviceManager->activityLog().info(
            LogCategory::Profile,
            QStringLiteral(
                "Scheduled apply skipped or failed %1 zone(s); unconfirmed hardware requires "
                "per-session confirmation from the GUI."
            ).arg(failedZones)
        );
    }
}

void ScheduleService::scheduleNextCheck()
{
    if (!m_config.enabled || m_config.profileName.isEmpty()) {
        m_timer.stop();
        return;
    }

    const qint64 untilNextRun = millisecondsUntilNextScheduleRun(
        currentDateTime(),
        parseScheduleTime(m_config.time)
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
