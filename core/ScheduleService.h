// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>

#include <functional>

namespace lumacore {

class DeviceManager;

class ScheduleService final : public QObject
{
    Q_OBJECT

public:
    struct Config {
        bool enabled {false};
        QString profileName;
        QString time {QStringLiteral("18:00")};
    };

    explicit ScheduleService(DeviceManager* deviceManager, QObject* parent = nullptr);

    [[nodiscard]] Config config() const;
    Config setConfig(const Config& config);
    void setClock(std::function<QDateTime()> clock);

public slots:
    void evaluateNow();

private:
    [[nodiscard]] static Config sanitizedConfig(Config config);
    [[nodiscard]] QDateTime currentDateTime() const;
    void loadPersistedConfig();
    void persistConfig();
    void applyScheduledProfile();
    void scheduleNextCheck();

    DeviceManager* m_deviceManager {nullptr};
    QSettings m_settings;
    Config m_config;
    QTimer m_timer;
    QDate m_lastAttemptDate;
    bool m_skipMissedRun {true};
    std::function<QDateTime()> m_clock;
};

} // namespace lumacore
