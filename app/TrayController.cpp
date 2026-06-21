#include "app/TrayController.h"

#include "ui/SettingsController.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWindow>

namespace lumacore {

TrayController::TrayController(
    QWindow* window,
    SettingsController* settingsController,
    const QIcon& applicationIcon,
    QObject* parent
)
    : QObject(parent)
    , m_window(window)
    , m_settingsController(settingsController)
{
    if (m_window != nullptr) {
        m_window->installEventFilter(this);
        connect(m_window, &QWindow::visibilityChanged, this, [this] { updateWindowAction(); });
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        if (m_settingsController != nullptr) {
            m_settingsController->setTrayAvailable(false);
        }
        QApplication::setQuitOnLastWindowClosed(true);
        return;
    }

    m_menu = std::make_unique<QMenu>();
    m_windowAction = m_menu->addAction(QStringLiteral("Hide LumaCore"));
    connect(m_windowAction, &QAction::triggered, this, &TrayController::toggleWindow);
    m_menu->addSeparator();
    QAction* quitAction = m_menu->addAction(QStringLiteral("Quit LumaCore"));
    connect(quitAction, &QAction::triggered, this, &TrayController::quitApplication);

    m_trayIcon = std::make_unique<QSystemTrayIcon>(applicationIcon);
    m_trayIcon->setToolTip(QStringLiteral("LumaCore"));
    m_trayIcon->setContextMenu(m_menu.get());
    connect(
        m_trayIcon.get(),
        &QSystemTrayIcon::activated,
        this,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                toggleWindow();
            }
        }
    );
    m_trayIcon->show();

    if (m_settingsController != nullptr) {
        m_settingsController->setTrayAvailable(true);
        connect(
            m_settingsController,
            &SettingsController::closeToTrayChanged,
            this,
            &TrayController::applyCloseBehavior
        );
    }

    applyCloseBehavior();
    updateWindowAction();
}

TrayController::~TrayController()
{
    if (m_window != nullptr) {
        m_window->removeEventFilter(this);
    }
}

bool TrayController::isAvailable() const
{
    return m_trayIcon != nullptr;
}

bool TrayController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window
        && event->type() == QEvent::Close
        && !m_quitRequested
        && isAvailable()
        && m_settingsController != nullptr
        && m_settingsController->closeToTray()) {
        static_cast<QCloseEvent*>(event)->ignore();
        m_window->hide();
        updateWindowAction();
        return true;
    }

    return QObject::eventFilter(watched, event);
}

void TrayController::applyCloseBehavior()
{
    const bool closeToTray = isAvailable()
        && m_settingsController != nullptr
        && m_settingsController->closeToTray();
    QApplication::setQuitOnLastWindowClosed(!closeToTray);
}

void TrayController::updateWindowAction()
{
    if (m_windowAction == nullptr) {
        return;
    }

    m_windowAction->setText(
        m_window != nullptr
            && m_window->isVisible()
            && m_window->visibility() != QWindow::Minimized
            ? QStringLiteral("Hide LumaCore")
            : QStringLiteral("Show LumaCore")
    );
}

void TrayController::toggleWindow()
{
    if (m_window == nullptr) {
        return;
    }

    if (m_window->isVisible() && m_window->visibility() != QWindow::Minimized) {
        m_window->hide();
    } else {
        showWindow();
    }
    updateWindowAction();
}

void TrayController::showWindow()
{
    if (m_window == nullptr) {
        return;
    }

    m_window->showNormal();
    m_window->raise();
    m_window->requestActivate();
}

void TrayController::quitApplication()
{
    m_quitRequested = true;
    QApplication::setQuitOnLastWindowClosed(true);
    if (m_trayIcon != nullptr) {
        m_trayIcon->hide();
    }
    QApplication::quit();
}

} // namespace lumacore
