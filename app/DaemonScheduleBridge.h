// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace lumacore {

class DaemonClient;
class SettingsController;

// Mirrors the GUI's schedule settings to a daemon that advertises schedule
// support: the scheduled profile is pushed with putProfile, then the schedule
// config with setSchedule. The GUI QSettings stay the source of truth; the
// daemon copy is overwritten on every (re)connect and settings change so it
// converges deterministically.
class DaemonScheduleBridge final : public QObject
{
    Q_OBJECT

public:
    DaemonScheduleBridge(
        SettingsController* settingsController,
        DaemonClient* daemonClient,
        QString profilesDirectoryPath,
        QObject* parent = nullptr
    );

    // Ownership is sticky across disconnects so a dropped daemon connection
    // does not resume local firing next to a daemon that still holds the
    // schedule. It clears only when a connected daemon stops advertising
    // support or answers a schedule call with an unknown-method error.
    [[nodiscard]] bool daemonOwnsSchedule() const;

signals:
    void daemonOwnsScheduleChanged(bool owned);

public slots:
    void notifyProfilesChanged();

private:
    void evaluateOwnershipDeferred();
    void evaluateOwnership();
    void setDaemonOwnsSchedule(bool owned);
    void schedulePush();
    void pushNow();
    void pushScheduleConfig();
    void handleScheduleCallError(const QString& error);

    SettingsController* m_settingsController {nullptr};
    DaemonClient* m_daemonClient {nullptr};
    QString m_profilesDirectoryPath;
    bool m_daemonOwnsSchedule {false};
    bool m_clientSupportSeen {false};
    bool m_evaluateQueued {false};
    bool m_pushQueued {false};
};

} // namespace lumacore
