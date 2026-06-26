// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/Version.h"
#include "backends/auto/AutoBackend.h"
#ifdef LUMACORE_HAS_ASUS_AURA_HID
#include "backends/asus/AsusAuraHidBackend.h"
#endif
#ifdef LUMACORE_HAS_LINUX_DISCOVERY
#include "backends/linux/LinuxDiscoveryBackend.h"
#endif
#include "backends/mock/MockBackend.h"
#include "core/DeviceManager.h"
#include "core/PortablePaths.h"
#include "daemon/DaemonOptions.h"
#include "ipc/DaemonServer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include <cstdio>
#include <memory>

namespace {

void configureQtApplication(QCoreApplication& application)
{
    application.setApplicationName(QStringLiteral("lumacore-daemon"));
    application.setApplicationVersion(lumacore::applicationVersion());
    application.setOrganizationName(QStringLiteral("LumaCore"));
    lumacore::configurePortableStorage();
}

bool validateEnvironment(const lumacore::DaemonOptions& options)
{
#ifdef Q_OS_LINUX
    if (::geteuid() != 0 && !options.allowUnprivileged) {
        constexpr char message[] =
            "lumacore-daemon must run as root. Use --allow-unprivileged only for local development.\n";
        std::fwrite(message, 1, sizeof(message) - 1, stderr);
        return false;
    }
#else
    Q_UNUSED(options)
#endif

    return true;
}

void registerBackends(lumacore::DeviceManager& deviceManager)
{
    deviceManager.registerBackend(std::make_unique<lumacore::AutoBackend>());
    deviceManager.registerBackend(std::make_unique<lumacore::MockBackend>());
#ifdef LUMACORE_HAS_LINUX_DISCOVERY
    deviceManager.registerBackend(std::make_unique<lumacore::LinuxDiscoveryBackend>());
#endif
#ifdef LUMACORE_HAS_ASUS_AURA_HID
    deviceManager.registerBackend(std::make_unique<lumacore::AsusAuraHidBackend>());
#endif
}

bool initializeRequestedBackend(
    lumacore::DeviceManager& deviceManager,
    const QString& requestedBackendId
)
{
    const QString backendId =
        requestedBackendId.isEmpty() ? lumacore::AutoBackend::backendId() : requestedBackendId;
    if (!deviceManager.backendRegistry().hasBackend(backendId)) {
        const QString message = QStringLiteral("Unknown backend '%1'. Available backends: %2")
                                    .arg(
                                        requestedBackendId,
                                        deviceManager.backendRegistry()
                                            .availableBackendIds()
                                            .join(QStringLiteral(", "))
                                    );
        qCritical().noquote() << message;
        std::fprintf(stderr, "%s\n", qPrintable(message));
        return false;
    }

    if (backendId == lumacore::AutoBackend::backendId()) {
        deviceManager.activityLog().info(
            lumacore::LogCategory::Backend,
            QStringLiteral("Auto backend will aggregate ASUS HID control, Linux discovery, and mock fallback.")
        );
    }

    deviceManager.initializeBackends(backendId);
    return true;
}

int runDaemon(const lumacore::DaemonOptions& options)
{
    if (!validateEnvironment(options)) {
        return 1;
    }

    lumacore::DeviceManager deviceManager;
    registerBackends(deviceManager);
    if (!initializeRequestedBackend(deviceManager, options.backendId)) {
        return 1;
    }

    lumacore::DaemonServer server(&deviceManager);
    server.setExitWhenIdle(options.exitOnDisconnect);
    QObject::connect(
        &server,
        &lumacore::DaemonServer::idleExitRequested,
        QCoreApplication::instance(),
        &QCoreApplication::quit
    );
    QString errorMessage;
    if (!server.listen(options.socketPath, &errorMessage)) {
        qCritical().noquote() << errorMessage;
        std::fprintf(stderr, "%s\n", qPrintable(errorMessage));
        return 1;
    }

    deviceManager.activityLog().info(
        lumacore::LogCategory::System,
        QStringLiteral("lumacore-daemon v%1 listening on %2.")
            .arg(lumacore::applicationVersion(), server.socketPath())
    );

    return QCoreApplication::exec();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    configureQtApplication(application);
    return runDaemon(lumacore::parseDaemonOptions(application));
}
