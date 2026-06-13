#pragma once

#include "core/RgbBackend.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace lumacore {

class BackendRegistry
{
public:
    void registerBackend(std::unique_ptr<RgbBackend> backend);
    [[nodiscard]] bool activateBackend(const QString& id);
    [[nodiscard]] RgbBackend* activeBackend() const;
    [[nodiscard]] QString activeBackendId() const;
    [[nodiscard]] BackendDescriptor activeDescriptor() const;
    [[nodiscard]] QStringList availableBackendIds() const;
    [[nodiscard]] bool hasBackend(const QString& id) const;

private:
    [[nodiscard]] RgbBackend* backendById(const QString& id) const;

    std::vector<std::unique_ptr<RgbBackend>> m_backends;
    QString m_activeBackendId;
};

} // namespace lumacore
