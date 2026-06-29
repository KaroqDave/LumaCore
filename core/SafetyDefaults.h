// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QtGlobal>

namespace lumacore {

// Windows starts dry-run-first so real RGB writes are never armed until the user
// explicitly opts in; other platforms default to applying writes. The daemon-side
// DeviceManager and the GUI-side SettingsController must agree on this default, so
// it lives here rather than being duplicated per translation unit.
[[nodiscard]] inline bool platformDefaultDryRunEnabled()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

} // namespace lumacore
