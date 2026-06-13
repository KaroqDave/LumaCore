#include "app/Version.h"
#include "backends/daemon/DaemonBackend.h"
#include "core/DeviceManager.h"
#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"
#include "ui/AppController.h"
#include "ui/DeviceTreeModel.h"
#include "ui/DeviceListModel.h"
#include "ui/SettingsController.h"
#include "ui/ZoneListModel.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSize>
#include <QUrl>
#include <QWindow>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdio>
#include <memory>

namespace {

QIcon createApplicationIcon()
{
    QIcon icon(QStringLiteral(":/icons/lumacore.ico"));
    icon.addFile(QStringLiteral(":/icons/lumacore-16.png"), QSize(16, 16));
    icon.addFile(QStringLiteral(":/icons/lumacore-32.png"), QSize(32, 32));
    icon.addFile(QStringLiteral(":/icons/lumacore-48.png"), QSize(48, 48));
    icon.addFile(QStringLiteral(":/icons/lumacore-64.png"), QSize(64, 64));
    icon.addFile(QStringLiteral(":/icons/lumacore-128.png"), QSize(128, 128));
    icon.addFile(QStringLiteral(":/icons/lumacore-256.png"), QSize(256, 256));
    return icon;
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
    if (::geteuid() == 0) {
        const QByteArray message = "LumaCore GUI must not run as root. Start lumacore-daemon as root instead.\n";
        std::fwrite(message.constData(), 1, static_cast<std::size_t>(message.size()), stderr);
        return 1;
    }
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LumaCore"));
    app.setApplicationDisplayName(QStringLiteral("LumaCore"));
    app.setApplicationVersion(lumacore::applicationVersion());
    app.setOrganizationName(QStringLiteral("LumaCore"));
    app.setWindowIcon(createApplicationIcon());
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("LumaCore RGB controller GUI."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption socketOption(
        QStringList {QStringLiteral("s"), QStringLiteral("socket")},
        QStringLiteral("LumaCore daemon Unix socket path."),
        QStringLiteral("path"),
        lumacore::defaultDaemonSocketPath()
    );
    parser.addOption(socketOption);
    parser.process(app);

    auto daemonClient = std::make_shared<lumacore::DaemonClient>(parser.value(socketOption));
    lumacore::DeviceManager deviceManager;
    deviceManager.registerBackend(std::make_unique<lumacore::DaemonBackend>(daemonClient));
    lumacore::DeviceListModel deviceListModel(&deviceManager);
    lumacore::DeviceTreeModel deviceTreeModel(&deviceManager);
    lumacore::ZoneListModel zoneListModel(&deviceManager);
    lumacore::AppController appController(&deviceManager, daemonClient);
    lumacore::SettingsController settingsController;

    appController.setDryRunEnabled(settingsController.dryRunEnabled());
    QObject::connect(
        &settingsController,
        &lumacore::SettingsController::dryRunEnabledChanged,
        &appController,
        [&settingsController, &appController]() { appController.setDryRunEnabled(settingsController.dryRunEnabled()); }
    );

    deviceManager.initializeBackends(QStringLiteral("daemon"));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError>& warnings) {
        for (const QQmlError& warning : warnings) {
            qCritical().noquote() << warning.toString();
            std::fprintf(stderr, "%s\n", qPrintable(warning.toString()));
        }
    });
    engine.rootContext()->setContextProperty(QStringLiteral("deviceModel"), &deviceListModel);
    engine.rootContext()->setContextProperty(QStringLiteral("deviceTreeModel"), &deviceTreeModel);
    engine.rootContext()->setContextProperty(QStringLiteral("zoneModel"), &zoneListModel);
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);
    engine.rootContext()->setContextProperty(QStringLiteral("settingsController"), &settingsController);
    QQmlComponent mainComponent(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/LumaCore/ui/qml/Main.qml")),
        QQmlComponent::PreferSynchronous
    );
    if (mainComponent.isError()) {
        for (const QQmlError& error : mainComponent.errors()) {
            qCritical().noquote() << error.toString();
            std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
        }
        return 1;
    }

    std::unique_ptr<QObject> rootObject(mainComponent.create());
    if (!rootObject) {
        for (const QQmlError& error : mainComponent.errors()) {
            qCritical().noquote() << error.toString();
            std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
        }
        if (mainComponent.errors().isEmpty()) {
            qCritical() << "Could not create LumaCore main window. Component status:" << mainComponent.status();
            std::fprintf(
                stderr,
                "Could not create LumaCore main window. Component status: %d\n",
                static_cast<int>(mainComponent.status())
            );
        }
        return 1;
    }

    if (auto* window = qobject_cast<QWindow*>(rootObject.get())) {
        window->setIcon(createApplicationIcon());
    }

    return QGuiApplication::exec();
}
