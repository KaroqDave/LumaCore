// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/VrrFlickerGuard.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace lumacore {

Q_LOGGING_CATEGORY(lcVrrFlickerGuard, "lumacore.app.vrrFlickerGuard", QtWarningMsg)

namespace {

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString visibilityText(QWindow::Visibility visibility)
{
    switch (visibility) {
    case QWindow::Hidden:
        return QStringLiteral("hidden");
    case QWindow::AutomaticVisibility:
        return QStringLiteral("automatic");
    case QWindow::Windowed:
        return QStringLiteral("windowed");
    case QWindow::Minimized:
        return QStringLiteral("minimized");
    case QWindow::Maximized:
        return QStringLiteral("maximized");
    case QWindow::FullScreen:
        return QStringLiteral("fullscreen");
    }

    return QStringLiteral("unknown");
}

QString refreshRateText(const QQuickWindow* window)
{
    if (window == nullptr || window->screen() == nullptr) {
        return QStringLiteral("unknown");
    }

    return QStringLiteral("%1 Hz").arg(window->screen()->refreshRate(), 0, 'f', 2);
}

QString guardModeText(bool enabled)
{
    return enabled ? QStringLiteral("maximum-stability") : QStringLiteral("off");
}

#ifdef Q_OS_WIN
bool isWindowOrTransientChild(QWindow* candidate, const QWindow* window)
{
    for (QWindow* current = candidate; current != nullptr; current = current->transientParent()) {
        if (current == window) {
            return true;
        }
    }

    return false;
}

bool isWindowOrTransientChildActive(const QWindow* window)
{
    if (window == nullptr) {
        return false;
    }

    if (window->isActive()) {
        return true;
    }

    return isWindowOrTransientChild(QGuiApplication::modalWindow(), window)
        || isWindowOrTransientChild(QGuiApplication::focusWindow(), window);
}
#endif

} // namespace

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
    QObject::connect(m_window, &QWindow::screenChanged, this, [this]() { reevaluate(); });
    QObject::connect(m_window, &QObject::destroyed, this, [this]() {
        m_window.clear();
        stopContinuousRendering();
    });
    m_window->installEventFilter(this);

#ifdef Q_OS_WIN
    if (QCoreApplication* app = QCoreApplication::instance()) {
        app->installNativeEventFilter(this);
    }
#endif

    reevaluate();
}

VrrFlickerGuard::~VrrFlickerGuard()
{
    stopContinuousRendering();
#ifdef Q_OS_WIN
    if (QCoreApplication* app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(this);
    }
#endif
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

bool VrrFlickerGuard::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window.data() && event->type() == QEvent::Expose) {
        reevaluate();
    }

    return QObject::eventFilter(watched, event);
}

void VrrFlickerGuard::reevaluate()
{
    if (m_window.isNull()) {
        stopContinuousRendering();
        return;
    }

#ifdef Q_OS_WIN
    const bool onScreen = m_window->isVisible()
        && m_window->isExposed()
        && m_window->visibility() != QWindow::Minimized
        && m_window->visibility() != QWindow::Hidden;
    const bool shouldRun = m_enabled
        && onScreen
        && isWindowOrTransientChildActive(m_window.data())
        && !m_interactiveMoveResize;
#else
    constexpr bool shouldRun = false;
#endif

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

    // Force the next Qt Quick repaint as soon as the current frame is presented.
    // QWindow::requestUpdate() is insufficient here because it does not force an
    // unchanged Qt Quick scene to render another frame. frameSwapped is emitted
    // on the render thread under the threaded render loop, so always route the
    // request back through this guard on the GUI thread.
    m_renderConnection = QObject::connect(
        m_window,
        &QQuickWindow::frameSwapped,
        this,
        [this]() {
            // A callback queued before stopContinuousRendering() may still run.
            if (m_running && !m_window.isNull()) {
                m_window->update();
            }
        },
        Qt::QueuedConnection
    );

    m_running = true;
    logContinuousRenderingTransition("started");
    m_window->update();
}

void VrrFlickerGuard::stopContinuousRendering()
{
    if (!m_running) {
        return;
    }

    QObject::disconnect(m_renderConnection);
    m_renderConnection = {};
    m_running = false;
    logContinuousRenderingTransition("stopped");
}

void VrrFlickerGuard::logContinuousRenderingTransition(const char* action) const
{
    if (m_window.isNull()) {
        qCInfo(lcVrrFlickerGuard).noquote()
            << QStringLiteral("VRR flicker guard %1: mode=%2 window=destroyed")
                   .arg(QString::fromLatin1(action), guardModeText(m_enabled));
        return;
    }

    qCInfo(lcVrrFlickerGuard).noquote()
        << QStringLiteral(
               "VRR flicker guard %1: mode=%2 refreshRate=%3 visible=%4 exposed=%5 active=%6 visibility=%7"
           )
               .arg(QString::fromLatin1(action),
                    guardModeText(m_enabled),
                    refreshRateText(m_window.data()),
                    boolText(m_window->isVisible()),
                    boolText(m_window->isExposed()),
                    boolText(m_window->isActive()),
                    visibilityText(m_window->visibility()));
}

#ifdef Q_OS_WIN
bool VrrFlickerGuard::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result)

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
        if (m_interactiveMoveResize) {
            return false;
        }
        m_interactiveMoveResize = true;
        reevaluate();
    } else if (msg->message == WM_EXITSIZEMOVE) {
        if (!m_interactiveMoveResize) {
            return false;
        }
        m_interactiveMoveResize = false;
        reevaluate();
    }

    return false;
}
#endif

} // namespace lumacore
