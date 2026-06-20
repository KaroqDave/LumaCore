#include "backends/mock/MockBackend.h"
#include "core/DeviceManager.h"
#include "ui/AppController.h"

#include <QCoreApplication>
#include <QDebug>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    lumacore::DeviceManager manager;
    manager.registerBackend(std::make_unique<lumacore::MockBackend>());
    manager.initializeBackends(QStringLiteral("mock"));

    lumacore::AppController controller(&manager);
    if (!require(controller.backendId() == QStringLiteral("mock"), "controller should expose the active backend")
        || !require(controller.backendDeviceCount() == 1, "controller should expose the loaded device count")
        || !require(controller.zoneCount(0) > 0, "mock device should expose zones")
        || !require(controller.zoneCount(-1) == 0, "invalid devices should expose no zones")
        || !require(controller.zoneName(-1, -1).isEmpty(), "invalid zones should expose an empty name")
        || !require(controller.zoneLedCount(-1, -1) == 0, "invalid zones should expose zero LEDs")
        || !require(
            controller.zoneEffectType(-1, -1) == static_cast<int>(lumacore::RgbEffectType::Static),
            "invalid zones should preserve the static-effect fallback"
        )
        || !require(controller.deviceSupportsEffect(0, 0), "mock devices should support static effects")
        || !require(controller.deviceSupportsEffect(0, 1), "mock devices should support animated effects")
        || !require(!controller.deviceSupportsEffect(0, 99), "unknown effects should remain unsupported")) {
        return 1;
    }

    return 0;
}
