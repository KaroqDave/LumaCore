// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/GuiApplication.h"

#include "app/DaemonLauncher.h"
#include "app/DaemonScheduleBridge.h"
#include "app/ProfileScheduleRunner.h"
#include "app/TrayController.h"
#include "app/Version.h"
#include "backends/daemon/DaemonBackend.h"
#include "core/PortablePaths.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QSize>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include <cstdio>
#include <memory>

namespace {

QIcon createApplicationIcon()
{
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/lumacore-16.png"), QSize(16, 16));
    icon.addFile(QStringLiteral(":/icons/lumacore-32.png"), QSize(32, 32));
    icon.addFile(QStringLiteral(":/icons/lumacore-48.png"), QSize(48, 48));
    icon.addFile(QStringLiteral(":/icons/lumacore-64.png"), QSize(64, 64));
    icon.addFile(QStringLiteral(":/icons/lumacore-128.png"), QSize(128, 128));
    icon.addFile(QStringLiteral(":/icons/lumacore-256.png"), QSize(256, 256));
    return icon;
}

} // namespace

namespace lumacore {

GuiApplication::BackendContext::BackendContext(const QString& socketPath)
    : daemonClient(std::make_shared<DaemonClient>(socketPath))
{
    deviceManager.registerBackend(std::make_unique<DaemonBackend>(daemonClient));
}

GuiApplication::GuiApplication(QApplication& qtApplication, const GuiOptions& options)
    : m_applicationIcon(qtApplication.windowIcon())
    , m_backendContext(options.daemonSocketPath)
    , m_deviceTreeModel(&m_backendContext.deviceManager)
    , m_appController(&m_backendContext.deviceManager, m_backendContext.daemonClient)
    , m_autoStartDaemon(options.autoStartDaemon)
{
    m_daemonLauncher = std::make_unique<DaemonLauncher>(m_backendContext.daemonClient);
    m_appController.setDryRunEnabled(m_settingsController.dryRunEnabled());
    QObject::connect(
        &m_settingsController,
        &SettingsController::dryRunEnabledChanged,
        &m_appController,
        [this]() { m_appController.setDryRunEnabled(m_settingsController.dryRunEnabled()); }
    );
}

GuiApplication::~GuiApplication()
{
    m_backendContext.daemonClient->setAutomaticReconnectEnabled(false);
    m_backendContext.daemonClient->disconnectFromDaemon();
    if (m_daemonLauncher != nullptr) {
        const bool exited = m_daemonLauncher->waitForStartedDaemonExit(1000);
        Q_UNUSED(exited)
    }
}

bool GuiApplication::validateEnvironment()
{
#ifdef Q_OS_LINUX
    if (::geteuid() == 0) {
        constexpr char message[] =
            "LumaCore GUI must not run as root. Start lumacore-daemon as root instead.\n";
        std::fwrite(message, 1, sizeof(message) - 1, stderr);
        return false;
    }
#endif

    return true;
}

void GuiApplication::configureQtApplication(QApplication& application)
{
    application.setApplicationName(QStringLiteral("LumaCore"));
    application.setApplicationDisplayName(QStringLiteral("LumaCore"));
    application.setApplicationVersion(applicationVersion());
    application.setOrganizationName(QStringLiteral("LumaCore"));
    configurePortableStorage();
    application.setWindowIcon(createApplicationIcon());
}

int GuiApplication::run()
{
    m_backendContext.deviceManager.initializeBackends(QStringLiteral("daemon"));
    m_deviceTreeModel.setWriteConfirmationSource(&m_appController);

    // Start the bundled daemon already in the user's persisted dry-run state so a
    // GUI-launched daemon never has a startup window in which its platform-default
    // dry-run state disagrees with the GUI before the post-connect sync completes.
    // Connection acquisition and the first device snapshot are asynchronous, so
    // the window appears immediately instead of blocking on daemon startup.
    m_daemonLauncher->ensureAvailableAsync(
        m_autoStartDaemon,
        {},
        m_settingsController.dryRunEnabled()
    );
    if (m_settingsController.applyOnLaunch()) {
        m_appController.armLaunchProfileApply(m_settingsController.activeProfile());
    }
    m_appController.enableDaemonRecovery();

    const QmlBindings bindings {
        .deviceTreeModel = &m_deviceTreeModel,
        .appController = &m_appController,
        .settingsController = &m_settingsController,
    };
    if (!m_qmlHost.load(bindings, m_applicationIcon, m_settingsController.startMinimized())) {
        return 1;
    }

    m_trayController = std::make_unique<TrayController>(
        m_qmlHost.mainWindow(),
        &m_settingsController,
        m_applicationIcon
    );
    m_profileScheduleRunner = std::make_unique<ProfileScheduleRunner>(
        &m_settingsController,
        &m_appController
    );
    m_daemonScheduleBridge = std::make_unique<DaemonScheduleBridge>(
        &m_settingsController,
        m_backendContext.daemonClient.get(),
        m_backendContext.deviceManager.profilesDirectoryPath()
    );
    QObject::connect(
        &m_appController,
        &AppController::profilesChanged,
        m_daemonScheduleBridge.get(),
        &DaemonScheduleBridge::notifyProfilesChanged
    );
    QObject::connect(
        m_daemonScheduleBridge.get(),
        &DaemonScheduleBridge::daemonOwnsScheduleChanged,
        m_profileScheduleRunner.get(),
        &ProfileScheduleRunner::setSuspended
    );
    m_profileScheduleRunner->setSuspended(m_daemonScheduleBridge->daemonOwnsSchedule());

    return QApplication::exec();
}

int GuiApplication::runSelfTest()
{
    // Headless QML smoke test. Load the real Main.qml with the production
    // controller bindings and fail on any component error or QML warning. This
    // exercises the initial render and Component.onCompleted paths that qmllint
    // cannot see. No daemon is launched or connected; the controllers report
    // their empty, disconnected state, which the interface must render cleanly.
    // Callers run this under QT_QPA_PLATFORM=offscreen.
    m_backendContext.deviceManager.initializeBackends(QStringLiteral("daemon"));
    m_deviceTreeModel.setWriteConfirmationSource(&m_appController);

    const QmlBindings bindings {
        .deviceTreeModel = &m_deviceTreeModel,
        .appController = &m_appController,
        .settingsController = &m_settingsController,
    };
    if (!m_qmlHost.load(bindings, m_applicationIcon, false)) {
        std::fprintf(stderr, "Self-test failed: the QML root did not load.\n");
        return 1;
    }

    // Let queued binding evaluations and Component.onCompleted handlers run so
    // any warnings they raise reach the engine before we inspect the count.
    for (int pass = 0; pass < 5; ++pass) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    const int warnings = m_qmlHost.warningCount();
    if (warnings > 0) {
        std::fprintf(stderr, "Self-test failed: QML load produced %d warning(s).\n", warnings);
        return 1;
    }

    std::fprintf(stdout, "Self-test passed: the QML interface loaded cleanly.\n");
    return 0;
}

} // namespace lumacore
