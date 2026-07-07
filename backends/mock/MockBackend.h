// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbBackend.h"
#include "core/RgbDevice.h"

#include <memory>
#include <vector>

#include <QStringList>

namespace lumacore {

class MockBackend final : public RgbBackend
{
public:
    explicit MockBackend(QString scenarioId = {});

    [[nodiscard]] static QString defaultScenarioId();
    [[nodiscard]] static QStringList scenarioIds();
    [[nodiscard]] static bool isKnownScenarioId(const QString& scenarioId);

    [[nodiscard]] BackendDescriptor descriptor() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;

private:
    QString m_scenarioId;
};

} // namespace lumacore
