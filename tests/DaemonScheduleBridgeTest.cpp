// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/DaemonScheduleBridge.h"
#include "core/DeviceManager.h"
#include "core/ProfileStore.h"
#include "core/ScheduleService.h"
#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"
#include "ui/SettingsController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>

#include <functional>
#include <memory>
#include <vector>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

QString testSocketName(const QString& suffix)
{
    return QStringLiteral("lumacore-schedule-bridge-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(suffix);
}

class BridgeDevice final : public lumacore::RgbDevice
{
public:
    BridgeDevice()
        : RgbDevice(
              QStringLiteral("bridge-device"),
              QStringLiteral("Bridge Device"),
              QStringLiteral("LumaCore"),
              lumacore::RgbDeviceType::Controller
          )
    {
        mutableZones().append(lumacore::RgbZone(
            QStringLiteral("Header"),
            lumacore::RgbZoneType::AddressableHeader,
            1
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        if (zoneIndex < 0 || zoneIndex >= mutableZones().size()) {
            return false;
        }
        mutableZones()[zoneIndex].setColor(color);
        return true;
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        return lumacore::BackendCapability::DiscoveryRead
            | lumacore::BackendCapability::ZoneColorWrite
            | lumacore::BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capabilities().testFlag(capability)) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        return {lumacore::PermissionStatus::Denied, QStringLiteral("Unsupported bridge test capability.")};
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    using namespace lumacore;

    QTemporaryDir settingsDirectory;
    QTemporaryDir guiProfilesDirectory;
    QTemporaryDir daemonProfilesDirectory;
    if (!require(
            settingsDirectory.isValid() && guiProfilesDirectory.isValid() && daemonProfilesDirectory.isValid(),
            "temporary bridge directories should be available"
        )) {
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("LumaCoreTests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    // GUI-side fixture: a profile authored in the GUI's own store.
    DeviceManager guiManager(nullptr, guiProfilesDirectory.filePath(QStringLiteral("profiles")));
    guiManager.setDryRunEnabled(false);
    std::vector<std::unique_ptr<RgbDevice>> guiDevices;
    guiDevices.push_back(std::make_unique<BridgeDevice>());
    guiManager.replaceDevices(std::move(guiDevices));
    if (!require(guiManager.setZoneStaticColor(0, 0, RgbColor(0x11, 0x22, 0x33)), "gui fixture color should apply")
        || !require(guiManager.saveProfile(QStringLiteral("Evening")), "gui fixture profile should save")) {
        return 1;
    }

    // The GUI settings and the daemon schedule service both persist through the
    // process-default QSettings; give each side its own application name so the
    // shared test process keeps them in separate files, as separate processes
    // would in production.
    QCoreApplication::setApplicationName(QStringLiteral("DaemonScheduleBridgeTestGui"));
    SettingsController settings;
    settings.setScheduledProfile(QStringLiteral("Evening"));
    settings.setScheduledProfileTime(QStringLiteral("07:30"));
    settings.setScheduledProfileEnabled(true);

    QCoreApplication::setApplicationName(QStringLiteral("DaemonScheduleBridgeTestDaemon"));
    DeviceManager daemonManager(nullptr, daemonProfilesDirectory.filePath(QStringLiteral("profiles")));
    ScheduleService scheduleService(&daemonManager);
    DaemonServer scheduleServer(&daemonManager);
    scheduleServer.setScheduleService(&scheduleService);
    const QString scheduleServerName = testSocketName(QStringLiteral("supported"));
    QString listenError;
    if (!require(scheduleServer.listen(scheduleServerName, &listenError), "bridge schedule server should listen")) {
        return 1;
    }

    DaemonClient client(scheduleServerName);
    DaemonScheduleBridge bridge(&settings, &client, guiManager.profilesDirectoryPath());
    int ownershipChanges = 0;
    bool lastOwnership = false;
    QObject::connect(
        &bridge,
        &DaemonScheduleBridge::daemonOwnsScheduleChanged,
        [&ownershipChanges, &lastOwnership](bool owned) {
            ++ownershipChanges;
            lastOwnership = owned;
        }
    );

    // A status handshake advertises schedule support, which hands ownership to
    // the daemon and mirrors the schedule and its profile content over.
    const DaemonCallResult status = client.call(daemonMethodName(DaemonMethod::Status), {}, 3000);
    if (!require(status.ok, "bridge status handshake should succeed")
        || !require(client.daemonScheduleSupported(), "client should record advertised schedule support")
        || !require(
            waitUntil([&bridge] { return bridge.daemonOwnsSchedule(); }),
            "advertised schedule support should hand ownership to the daemon"
        )
        || !require(ownershipChanges == 1 && lastOwnership, "ownership handoff should be signalled once")
        || !require(
            waitUntil([&scheduleService] {
                const ScheduleService::Config config = scheduleService.config();
                return config.enabled
                    && config.profileName == QStringLiteral("Evening")
                    && config.time == QStringLiteral("07:30");
            }),
            "the GUI schedule should be mirrored into the daemon service"
        )
        || !require(
            waitUntil([&daemonManager] {
                return ProfileStore(daemonManager.profilesDirectoryPath())
                    .names()
                    .contains(QStringLiteral("Evening"));
            }),
            "the scheduled profile should be pushed into the daemon store"
        )) {
        return 1;
    }

    // Settings changes re-push the config.
    settings.setScheduledProfileTime(QStringLiteral("08:15"));
    if (!require(
            waitUntil([&scheduleService] {
                return scheduleService.config().time == QStringLiteral("08:15");
            }),
            "schedule setting changes should re-push to the daemon"
        )) {
        return 1;
    }

    // Profile content changes re-push the profile.
    if (!require(guiManager.setZoneStaticColor(0, 0, RgbColor(0x44, 0x55, 0x66)), "updated fixture color should apply")
        || !require(guiManager.saveProfile(QStringLiteral("Evening")), "updated fixture profile should save")) {
        return 1;
    }
    bridge.notifyProfilesChanged();
    QJsonObject guiProfile;
    if (!require(
            ProfileStore(guiManager.profilesDirectoryPath()).load(QStringLiteral("Evening"), &guiProfile, nullptr),
            "updated gui profile should load"
        )
        || !require(
            waitUntil([&daemonManager, &guiProfile] {
                QJsonObject daemonProfile;
                return ProfileStore(daemonManager.profilesDirectoryPath())
                        .load(QStringLiteral("Evening"), &daemonProfile, nullptr)
                    && daemonProfile == guiProfile;
            }),
            "profile saves should re-push content to the daemon store"
        )) {
        return 1;
    }

    // Ownership stays sticky while the daemon connection is down, so the local
    // runner cannot double-fire next to a daemon that still holds the schedule.
    scheduleServer.close();
    if (!require(
            waitUntil([&client] { return !client.isConnected(); }),
            "closing the server should disconnect the bridge client"
        )) {
        return 1;
    }
    QCoreApplication::processEvents();
    if (!require(bridge.daemonOwnsSchedule(), "schedule ownership should stay sticky across disconnects")) {
        return 1;
    }

    // Reconnecting to a daemon without schedule support hands the schedule back
    // to the local runner.
    DaemonServer plainServer(&daemonManager);
    const QString plainServerName = testSocketName(QStringLiteral("unsupported"));
    QString plainListenError;
    if (!require(plainServer.listen(plainServerName, &plainListenError), "plain server should listen")) {
        return 1;
    }
    client.setSocketPath(plainServerName);
    const DaemonCallResult plainStatus = client.call(daemonMethodName(DaemonMethod::Status), {}, 3000);
    if (!require(plainStatus.ok, "plain status handshake should succeed")
        || !require(
            waitUntil([&bridge] { return !bridge.daemonOwnsSchedule(); }),
            "a daemon without schedule support should hand ownership back to the runner"
        )
        || !require(!lastOwnership, "the ownership release should be signalled")) {
        return 1;
    }

    return 0;
}
