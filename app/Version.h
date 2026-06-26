// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

#ifndef LUMACORE_VERSION
#define LUMACORE_VERSION "1.1.7.1"
#endif

namespace lumacore {

inline QString applicationVersion()
{
    return QLatin1String(LUMACORE_VERSION);
}

} // namespace lumacore
