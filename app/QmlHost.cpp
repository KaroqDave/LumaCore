#include "app/QmlHost.h"

#include "app/VrrFlickerGuard.h"
#include "ui/SettingsController.h"

#include <QDebug>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickWindow>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>
#include <QWindow>

#include <cstdio>

namespace {

void reportQmlErrors(const QList<QQmlError>& errors)
{
    for (const QQmlError& error : errors) {
        qCritical().noquote() << error.toString();
        std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
    }
}

} // namespace

namespace lumacore {

QmlHost::QmlHost()
{
    QObject::connect(&m_engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError>& warnings) {
        reportQmlErrors(warnings);
    });
}

QmlHost::~QmlHost() = default;

QWindow* QmlHost::mainWindow() const
{
    return qobject_cast<QWindow*>(m_rootObject.get());
}

bool QmlHost::load(const QmlBindings& bindings, const QIcon& applicationIcon, bool startMinimized)
{
    m_mainComponent = std::make_unique<QQmlComponent>(
        &m_engine,
        QUrl(QStringLiteral("qrc:/qt/qml/LumaCore/ui/qml/Main.qml")),
        QQmlComponent::PreferSynchronous
    );
    if (m_mainComponent->isError()) {
        reportQmlErrors(m_mainComponent->errors());
        return false;
    }

    const QVariantMap initialProperties {
        {QStringLiteral("deviceTreeModel"), QVariant::fromValue(bindings.deviceTreeModel)},
        {QStringLiteral("appController"), QVariant::fromValue(bindings.appController)},
        {QStringLiteral("settingsController"), QVariant::fromValue(static_cast<QObject*>(bindings.settingsController))},
        {QStringLiteral("startMinimized"), startMinimized},
    };
    m_rootObject.reset(m_mainComponent->createWithInitialProperties(initialProperties));
    if (!m_rootObject) {
        reportQmlErrors(m_mainComponent->errors());
        if (m_mainComponent->errors().isEmpty()) {
            qCritical() << "Could not create LumaCore main window. Component status:"
                        << m_mainComponent->status();
            std::fprintf(
                stderr,
                "Could not create LumaCore main window. Component status: %d\n",
                static_cast<int>(m_mainComponent->status())
            );
        }
        return false;
    }

    if (auto* window = qobject_cast<QWindow*>(m_rootObject.get())) {
        window->setIcon(applicationIcon);
    }

#ifdef Q_OS_WIN
    if (auto* quickWindow = qobject_cast<QQuickWindow*>(m_rootObject.get())) {
        m_vrrFlickerGuard = std::make_unique<VrrFlickerGuard>(quickWindow);
        if (bindings.settingsController != nullptr) {
            SettingsController* settings = bindings.settingsController;
            m_vrrFlickerGuard->setEnabled(settings->reduceVrrFlicker());
            QObject::connect(
                settings,
                &SettingsController::reduceVrrFlickerChanged,
                m_vrrFlickerGuard.get(),
                [this, settings]() { m_vrrFlickerGuard->setEnabled(settings->reduceVrrFlicker()); }
            );
        }
    }
#endif

    return true;
}

} // namespace lumacore
