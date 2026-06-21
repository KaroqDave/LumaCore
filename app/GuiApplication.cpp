#include "app/GuiApplication.h"

#include "app/DaemonLauncher.h"
#include "app/Version.h"
#include "backends/daemon/DaemonBackend.h"

#include <QGuiApplication>
#include <QSize>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <unistd.h>
#include <cstdio>
#endif

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

GuiApplication::GuiApplication(QGuiApplication& qtApplication, const GuiOptions& options)
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

void GuiApplication::configureQtApplication(QGuiApplication& application)
{
    application.setApplicationName(QStringLiteral("LumaCore"));
    application.setApplicationDisplayName(QStringLiteral("LumaCore"));
    application.setApplicationVersion(applicationVersion());
    application.setOrganizationName(QStringLiteral("LumaCore"));
    application.setWindowIcon(createApplicationIcon());
}

int GuiApplication::run()
{
    const bool daemonAvailable = m_daemonLauncher->ensureAvailable(m_autoStartDaemon);
    m_backendContext.deviceManager.initializeBackends(QStringLiteral("daemon"));
    if (!daemonAvailable && !m_daemonLauncher->lastError().isEmpty()) {
        m_backendContext.daemonClient->reportConnectionError(m_daemonLauncher->lastError());
    }
    if (m_settingsController.applyOnLaunch()) {
        const bool profileApplied =
            m_appController.applyProfileOnLaunch(m_settingsController.activeProfile());
        Q_UNUSED(profileApplied)
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

    return QGuiApplication::exec();
}

} // namespace lumacore
