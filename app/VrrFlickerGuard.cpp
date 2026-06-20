#include "app/VrrFlickerGuard.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace lumacore {

VrrFlickerGuard::VrrFlickerGuard(QQuickWindow* window, QObject* parent)
    : QObject(parent)
    , m_window(window)
{
    if (m_window == nullptr) {
        return;
    }

    // Re-evaluate whenever the window appears, hides, minimizes, or gains/loses
    // focus so continuous rendering tracks the window state instead of running
    // for the whole lifetime of the process.
    QObject::connect(m_window, &QWindow::visibilityChanged, this, [this]() { reevaluate(); });
    QObject::connect(m_window, &QWindow::activeChanged, this, [this]() { reevaluate(); });

    if (QCoreApplication* app = QCoreApplication::instance()) {
        app->installNativeEventFilter(this);
    }

    reevaluate();
}

VrrFlickerGuard::~VrrFlickerGuard()
{
    if (QCoreApplication* app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(this);
    }
    stopContinuousRendering();
}

void VrrFlickerGuard::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    reevaluate();
}

bool VrrFlickerGuard::isEnabled() const
{
    return m_enabled;
}

void VrrFlickerGuard::reevaluate()
{
    if (m_window.isNull()) {
        stopContinuousRendering();
        return;
    }

    const bool onScreen = m_window->isVisible()
        && m_window->visibility() != QWindow::Minimized
        && m_window->visibility() != QWindow::Hidden;
    const bool shouldRun = m_enabled && onScreen && m_window->isActive() && !m_interactiveMoveResize;

    if (shouldRun) {
        startContinuousRendering();
    } else {
        stopContinuousRendering();
    }
}

void VrrFlickerGuard::startContinuousRendering()
{
    if (m_running || m_window.isNull()) {
        return;
    }

    // Request the next frame as soon as the current one is presented. frameSwapped
    // is emitted on the render thread under the threaded render loop, so route the
    // request back through this guard (GUI thread) via a queued connection.
    m_renderConnection = QObject::connect(
        m_window,
        &QQuickWindow::frameSwapped,
        this,
        [this]() {
            if (!m_window.isNull()) {
                m_window->requestUpdate();
            }
        }
    );

    m_running = true;
    m_window->requestUpdate();
}

void VrrFlickerGuard::stopContinuousRendering()
{
    if (!m_running) {
        return;
    }

    QObject::disconnect(m_renderConnection);
    m_renderConnection = {};
    m_running = false;
}

bool VrrFlickerGuard::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result)

#ifdef Q_OS_WIN
    if (m_window.isNull() || eventType != QByteArrayLiteral("windows_generic_MSG")) {
        return false;
    }

    const MSG* msg = static_cast<const MSG*>(message);
    if (msg->hwnd != reinterpret_cast<HWND>(m_window->winId())) {
        return false;
    }

    // Pause continuous rendering for the duration of the modal move/resize loop so
    // the drag stays smooth, then restore it once the loop ends.
    if (msg->message == WM_ENTERSIZEMOVE) {
        m_interactiveMoveResize = true;
        reevaluate();
    } else if (msg->message == WM_EXITSIZEMOVE) {
        m_interactiveMoveResize = false;
        reevaluate();
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif

    return false;
}

} // namespace lumacore
