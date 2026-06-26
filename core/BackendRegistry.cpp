// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/BackendRegistry.h"

namespace lumacore {

void BackendRegistry::registerBackend(std::unique_ptr<RgbBackend> backend)
{
    if (!backend) {
        return;
    }

    const QString id = backend->descriptor().id;
    if (id.isEmpty()) {
        return;
    }

    for (const std::unique_ptr<RgbBackend>& existing : m_backends) {
        if (existing && existing->descriptor().id == id) {
            return;
        }
    }

    m_backends.push_back(std::move(backend));
}

bool BackendRegistry::activateBackend(const QString& id)
{
    if (!hasBackend(id)) {
        return false;
    }

    m_activeBackendId = id;
    return true;
}

RgbBackend* BackendRegistry::activeBackend() const
{
    return backendById(m_activeBackendId);
}

QString BackendRegistry::activeBackendId() const
{
    return m_activeBackendId;
}

BackendDescriptor BackendRegistry::activeDescriptor() const
{
    const RgbBackend* backend = activeBackend();
    if (backend == nullptr) {
        return {};
    }

    return backend->descriptor();
}

QStringList BackendRegistry::availableBackendIds() const
{
    QStringList ids;
    for (const std::unique_ptr<RgbBackend>& backend : m_backends) {
        if (backend) {
            ids.append(backend->descriptor().id);
        }
    }
    return ids;
}

bool BackendRegistry::hasBackend(const QString& id) const
{
    return backendById(id) != nullptr;
}

RgbBackend* BackendRegistry::backendById(const QString& id) const
{
    if (id.isEmpty()) {
        return nullptr;
    }

    for (const std::unique_ptr<RgbBackend>& backend : m_backends) {
        if (backend && backend->descriptor().id == id) {
            return backend.get();
        }
    }

    return nullptr;
}

} // namespace lumacore
