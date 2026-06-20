#pragma once

#include "app/GuiOptions.h"
#include "app/QmlHost.h"
#include "core/DeviceManager.h"
#include "ipc/DaemonClient.h"
#include "ui/AppController.h"
#include "ui/DeviceTreeModel.h"
#include "ui/SettingsController.h"

#include <QIcon>

#include <memory>

class QGuiApplication;

namespace lumacore {

class DaemonLauncher;

class GuiApplication final
{
public:
    GuiApplication(QGuiApplication& qtApplication, const GuiOptions& options);
    ~GuiApplication();

    [[nodiscard]] static bool validateEnvironment();
    static void configureQtApplication(QGuiApplication& application);
    [[nodiscard]] int run();

private:
    struct BackendContext {
        explicit BackendContext(const QString& socketPath);

        std::shared_ptr<DaemonClient> daemonClient;
        DeviceManager deviceManager;
    };

    QIcon m_applicationIcon;
    std::unique_ptr<DaemonLauncher> m_daemonLauncher;
    BackendContext m_backendContext;
    DeviceTreeModel m_deviceTreeModel;
    AppController m_appController;
    SettingsController m_settingsController;
    QmlHost m_qmlHost;
    bool m_autoStartDaemon {false};
};

} // namespace lumacore
