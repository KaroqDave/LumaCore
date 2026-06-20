#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

class QQuickWindow;

namespace lumacore {

// On Windows, keeps a Qt Quick window presenting at the monitor's refresh rate
// while it is visible and focused. Qt Quick renders on demand, so an idle window
// presents almost no frames while interaction or animation bursts present at
// vsync. On a variable-refresh-rate display (NVIDIA G-Sync / AMD FreeSync /
// Adaptive-Sync) the desktop refresh rate follows the focused window, so those
// swings make the panel refresh rate jump around, which is seen as brightness
// flicker.
//
// While active, this guard chains a fresh update request to every presented
// frame, pinning the present rate to the refresh rate so the panel stays at a
// stable rate and stops flickering. It only runs while the window is on screen
// and focused so a backgrounded window does not keep the GPU busy.
class VrrFlickerGuard final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit VrrFlickerGuard(QQuickWindow* window, QObject* parent = nullptr);
    ~VrrFlickerGuard() override;

    VrrFlickerGuard(const VrrFlickerGuard&) = delete;
    VrrFlickerGuard& operator=(const VrrFlickerGuard&) = delete;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

    // Catches the platform's interactive move/resize loop so continuous rendering
    // can be paused while the user drags the window. Forcing a frame every vsync
    // during the modal move loop fights the GUI thread and makes the drag stutter.
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void reevaluate();
    void startContinuousRendering();
    void stopContinuousRendering();

    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_renderConnection;
    bool m_enabled {false};
    bool m_running {false};
    bool m_interactiveMoveResize {false};
};

} // namespace lumacore
