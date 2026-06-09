#include "core/DeviceManager.h"
#include "ui/AppController.h"
#include "ui/DeviceListModel.h"
#include "ui/ZoneListModel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LumaCore"));
    app.setOrganizationName(QStringLiteral("LumaCore"));

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

    return QGuiApplication::exec();
}
