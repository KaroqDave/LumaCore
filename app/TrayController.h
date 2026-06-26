// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QPointer>

#include <memory>

class QAction;
class QEvent;
class QIcon;
class QMenu;
class QSystemTrayIcon;
class QWindow;

namespace lumacore {

class SettingsController;

class TrayController final : public QObject
{
    Q_OBJECT

public:
    TrayController(
        QWindow* window,
        SettingsController* settingsController,
        const QIcon& applicationIcon,
        QObject* parent = nullptr
    );
    ~TrayController() override;

    [[nodiscard]] bool isAvailable() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyCloseBehavior();
    void updateWindowAction();
    void toggleWindow();
    void showWindow();
    void quitApplication();

    QPointer<QWindow> m_window;
    QPointer<SettingsController> m_settingsController;
    std::unique_ptr<QMenu> m_menu;
    std::unique_ptr<QSystemTrayIcon> m_trayIcon;
    QAction* m_windowAction {nullptr};
    bool m_quitRequested {false};
};

} // namespace lumacore
