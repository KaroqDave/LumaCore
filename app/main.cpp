#include "core/DeviceManager.h"
#include "ui/DeviceListModel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LumaCore"));
    app.setOrganizationName(QStringLiteral("LumaCore"));

    lumacore::DeviceManager deviceManager;
    deviceManager.initializeMockDevices();

    lumacore::DeviceListModel deviceListModel(&deviceManager);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("deviceModel"), &deviceListModel);
    engine.loadFromModule(QStringLiteral("LumaCore"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return QGuiApplication::exec();
}
