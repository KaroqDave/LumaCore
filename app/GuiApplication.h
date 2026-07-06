// SPDX-License-Identifier: GPL-2.0-or-later

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

class QApplication;

namespace lumacore {

class DaemonLauncher;
class DaemonScheduleBridge;
class ProfileScheduleRunner;
class TrayController;

class GuiApplication final
{
public:
    GuiApplication(QApplication& qtApplication, const GuiOptions& options);
    ~GuiApplication();

    [[nodiscard]] static bool validateEnvironment();
    static void configureQtApplication(QApplication& application);
    [[nodiscard]] int run();
    // Load the QML interface once with the production bindings, without a live
    // daemon or event loop, and return non-zero on any load or binding error.
    [[nodiscard]] int runSelfTest();

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
    std::unique_ptr<TrayController> m_trayController;
    std::unique_ptr<ProfileScheduleRunner> m_profileScheduleRunner;
    std::unique_ptr<DaemonScheduleBridge> m_daemonScheduleBridge;
    QString m_mockScenarioId;
    bool m_autoStartDaemon {false};
};

} // namespace lumacore
