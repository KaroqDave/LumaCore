#pragma once

#include <QString>

namespace lumacore {

[[nodiscard]] QString portableDataRoot();
[[nodiscard]] QString portableSettingsDirectory();
[[nodiscard]] QString portableProfilesDirectory();
[[nodiscard]] QString portableCacheDirectory();
void configurePortableStorage();

} // namespace lumacore
