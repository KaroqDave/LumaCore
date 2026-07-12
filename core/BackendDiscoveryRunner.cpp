// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/BackendDiscoveryRunner.h"

#include <QThread>

#include <utility>

namespace lumacore {

BackendDiscoveryRunner::BackendDiscoveryRunner(QObject* parent)
    : QObject(parent)
{
}

BackendDiscoveryRunner::~BackendDiscoveryRunner()
{
    if (m_thread == nullptr) {
        return;
    }

    m_thread->disconnect(this);
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    m_completion = {};
    m_result.reset();
}

bool BackendDiscoveryRunner::start(
    const RgbBackend* backend,
    QThread* resultThread,
    CompletionHandler completion
)
{
    if (backend == nullptr || resultThread == nullptr || !completion || m_thread != nullptr) {
        return false;
    }

    m_result = std::make_shared<BackendDiscoveryResult>();
    m_completion = std::move(completion);
    const std::shared_ptr<BackendDiscoveryResult> result = m_result;
    m_thread = QThread::create(
        [backend, resultThread, result] {
            result->devices = backend->discoverDevices();
            result->error = backend->lastDiscoverError();
            for (std::unique_ptr<RgbDevice>& device : result->devices) {
                if (device != nullptr && device->thread() != resultThread) {
                    device->moveToThread(resultThread);
                }
            }
        }
    );
    connect(
        m_thread,
        &QThread::finished,
        this,
        [this] {
            QThread* completedThread = m_thread;
            m_thread = nullptr;
            if (completedThread != nullptr) {
                completedThread->deleteLater();
            }

            BackendDiscoveryResult result = std::move(*m_result);
            m_result.reset();
            CompletionHandler completion = std::move(m_completion);
            completion(std::move(result));
        }
    );
    m_thread->start();
    return true;
}

bool BackendDiscoveryRunner::isRunning() const
{
    return m_thread != nullptr;
}

} // namespace lumacore
