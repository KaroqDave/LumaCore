#pragma once

#include <QIcon>
#include <QObject>
#include <QQmlApplicationEngine>

#include <memory>

class QQmlComponent;

namespace lumacore {

class SettingsController;
class VrrFlickerGuard;

struct QmlBindings {
    QObject* deviceTreeModel {nullptr};
    QObject* appController {nullptr};
    SettingsController* settingsController {nullptr};
};

class QmlHost final
{
public:
    QmlHost();
    ~QmlHost();

    QmlHost(const QmlHost&) = delete;
    QmlHost& operator=(const QmlHost&) = delete;

    [[nodiscard]] bool load(
        const QmlBindings& bindings,
        const QIcon& applicationIcon,
        bool startMinimized
    );

private:
    QQmlApplicationEngine m_engine;
    std::unique_ptr<QQmlComponent> m_mainComponent;
    std::unique_ptr<QObject> m_rootObject;
    std::unique_ptr<VrrFlickerGuard> m_vrrFlickerGuard;
};

} // namespace lumacore
