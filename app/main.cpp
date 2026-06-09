#include "app/Version.h"
#include "core/DeviceManager.h"
#include "ui/AppController.h"
#include "ui/DeviceListModel.h"
#include "ui/ZoneListModel.h"

#include <QGuiApplication>
#include <QIcon>
#include <QSize>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

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
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LumaCore"));
    app.setApplicationDisplayName(QStringLiteral("LumaCore"));
    app.setApplicationVersion(lumacore::applicationVersion());
    app.setOrganizationName(QStringLiteral("LumaCore"));
    app.setWindowIcon(createApplicationIcon());

    lumacore::DeviceManager deviceManager;
    lumacore::DeviceListModel deviceListModel(&deviceManager);
    lumacore::ZoneListModel zoneListModel(&deviceManager);
    lumacore::AppController appController(&deviceManager);

    deviceManager.initializeMockDevices();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("deviceModel"), &deviceListModel);
    engine.rootContext()->setContextProperty(QStringLiteral("zoneModel"), &zoneListModel);
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);
    engine.loadFromModule(QStringLiteral("LumaCore"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().constFirst())) {
        window->setIcon(createApplicationIcon());
    }

    return QGuiApplication::exec();
}
