// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/DaemonScheduleBridge.h"

#include "core/ProfileStore.h"
#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"
#include "ui/SettingsController.h"

#include <QJsonObject>
#include <QPointer>
#include <QTimer>

namespace lumacore {

DaemonScheduleBridge::DaemonScheduleBridge(
    SettingsController* settingsController,
    DaemonClient* daemonClient,
    QString profilesDirectoryPath,
    QObject* parent
)
    : QObject(parent)
    , m_settingsController(settingsController)
    , m_daemonClient(daemonClient)
    , m_profilesDirectoryPath(std::move(profilesDirectoryPath))
{
    if (m_daemonClient != nullptr) {
        connect(
            m_daemonClient,
            &DaemonClient::daemonInfoChanged,
            this,
            &DaemonScheduleBridge::evaluateOwnershipDeferred
        );
        connect(
            m_daemonClient,
            &DaemonClient::connectionStateChanged,
            this,
            &DaemonScheduleBridge::evaluateOwnershipDeferred
        );
    }
    if (m_settingsController != nullptr) {
        connect(
            m_settingsController,
            &SettingsController::scheduledProfileEnabledChanged,
            this,
            &DaemonScheduleBridge::schedulePush
        );
        connect(
            m_settingsController,
            &SettingsController::scheduledProfileChanged,
            this,
            &DaemonScheduleBridge::schedulePush
        );
        connect(
            m_settingsController,
            &SettingsController::scheduledProfileTimeChanged,
            this,
            &DaemonScheduleBridge::schedulePush
        );
    }

    evaluateOwnershipDeferred();
}

bool DaemonScheduleBridge::daemonOwnsSchedule() const
{
    return m_daemonOwnsSchedule;
}

void DaemonScheduleBridge::notifyProfilesChanged()
{
    schedulePush();
}

void DaemonScheduleBridge::evaluateOwnershipDeferred()
{
    // The client sets version, dry-run, and schedule support one field at a
    // time while processing a status result, emitting daemonInfoChanged for
    // each. Deferring coalesces those intermediate states so ownership is
    // only judged against a fully-updated snapshot.
    if (m_evaluateQueued) {
        return;
    }
    m_evaluateQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_evaluateQueued = false;
        evaluateOwnership();
    });
}

void DaemonScheduleBridge::evaluateOwnership()
{
    if (m_daemonClient == nullptr) {
        return;
    }

    const bool supported = m_daemonClient->daemonScheduleSupported();
    if (supported && !m_clientSupportSeen) {
        m_clientSupportSeen = true;
        setDaemonOwnsSchedule(true);
        schedulePush();
        return;
    }
    if (!supported) {
        m_clientSupportSeen = false;
        // A connected daemon that has completed its handshake but does not
        // advertise support is an older daemon; hand the schedule back to the
        // local runner. A disconnected client keeps ownership sticky.
        if (m_daemonClient->isConnected() && !m_daemonClient->daemonVersion().isEmpty()) {
            setDaemonOwnsSchedule(false);
        }
    }
}

void DaemonScheduleBridge::setDaemonOwnsSchedule(bool owned)
{
    if (m_daemonOwnsSchedule == owned) {
        return;
    }

    m_daemonOwnsSchedule = owned;
    emit daemonOwnsScheduleChanged(owned);
}

void DaemonScheduleBridge::schedulePush()
{
    if (!m_daemonOwnsSchedule || m_pushQueued) {
        return;
    }
    m_pushQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_pushQueued = false;
        pushNow();
    });
}

void DaemonScheduleBridge::pushNow()
{
    if (!m_daemonOwnsSchedule || m_daemonClient == nullptr || m_settingsController == nullptr) {
        return;
    }

    const QString profileName = m_settingsController->scheduledProfile();
    if (profileName.isEmpty()) {
        pushScheduleConfig();
        return;
    }

    QJsonObject profile;
    const ProfileStore profileStore(m_profilesDirectoryPath);
    if (!profileStore.load(ProfileStore::normalizeName(profileName), &profile, nullptr)) {
        // Push the config anyway so the daemon mirrors the GUI state; it will
        // report the profile as unavailable until a save re-pushes it.
        pushScheduleConfig();
        return;
    }

    const QPointer<DaemonScheduleBridge> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::PutProfile),
        {
            {QStringLiteral("profileName"), profileName},
            {QStringLiteral("profile"), profile},
        },
        [self](DaemonCallResult response) {
            if (self == nullptr) {
                return;
            }
            if (!response.ok) {
                self->handleScheduleCallError(response.error);
                return;
            }
            self->pushScheduleConfig();
        }
    );
    Q_UNUSED(requestId)
}

void DaemonScheduleBridge::pushScheduleConfig()
{
    if (!m_daemonOwnsSchedule || m_daemonClient == nullptr || m_settingsController == nullptr) {
        return;
    }

    const QPointer<DaemonScheduleBridge> self(this);
    const quint64 requestId = m_daemonClient->callAsync(
        daemonMethodName(DaemonMethod::SetSchedule),
        {
            {QStringLiteral("enabled"), m_settingsController->scheduledProfileEnabled()},
            {QStringLiteral("profileName"), m_settingsController->scheduledProfile()},
            {QStringLiteral("time"), m_settingsController->scheduledProfileTime()},
        },
        [self](DaemonCallResult response) {
            if (self == nullptr) {
                return;
            }
            if (!response.ok) {
                self->handleScheduleCallError(response.error);
            }
        }
    );
    Q_UNUSED(requestId)
}

void DaemonScheduleBridge::handleScheduleCallError(const QString& error)
{
    // A daemon that answers with an unknown-method error advertised support it
    // does not have (or was replaced mid-session); fall back to local firing.
    if (error.contains(QStringLiteral("Unknown daemon method"))) {
        m_clientSupportSeen = false;
        setDaemonOwnsSchedule(false);
    }
}

} // namespace lumacore
