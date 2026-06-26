// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

namespace lumacore {

[[nodiscard]] QString portableDataRoot();
[[nodiscard]] QString portableSettingsDirectory();
[[nodiscard]] QString portableProfilesDirectory();
[[nodiscard]] QString portableCacheDirectory();
void configurePortableStorage();

} // namespace lumacore
