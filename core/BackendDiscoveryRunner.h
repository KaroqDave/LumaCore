// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbBackend.h"
#include "core/RgbDevice.h"

#include <QObject>

#include <functional>
#include <memory>
#include <vector>

class QThread;

namespace lumacore {

struct BackendDiscoveryResult {
    std::vector<std::unique_ptr<RgbDevice>> devices;
    QString error;
};

class BackendDiscoveryRunner final : public QObject
{
public:
    using CompletionHandler = std::function<void(BackendDiscoveryResult)>;

    explicit BackendDiscoveryRunner(QObject* parent = nullptr);
    ~BackendDiscoveryRunner() override;

    [[nodiscard]] bool start(
        const RgbBackend* backend,
        QThread* resultThread,
        CompletionHandler completion
    );
    [[nodiscard]] bool isRunning() const;

private:
    QThread* m_thread {nullptr};
    std::shared_ptr<BackendDiscoveryResult> m_result;
    CompletionHandler m_completion;
};

} // namespace lumacore
