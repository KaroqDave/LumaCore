// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/RgbBackend.h"

#include <QSet>
#include <QString>

namespace lumacore {

class AutoBackend final : public RgbBackend
{
public:
    [[nodiscard]] static QString backendId();
    [[nodiscard]] static bool isRepresentedDiscoveryDevice(
        const RgbDevice& device,
        const QSet<QString>& representedIdentities
    );

    [[nodiscard]] BackendDescriptor descriptor() const override;
    [[nodiscard]] std::vector<std::unique_ptr<RgbDevice>> discoverDevices() const override;
    [[nodiscard]] PermissionResult probe() const override;
};

} // namespace lumacore
