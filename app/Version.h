#pragma once

#include <QString>

#ifndef LUMACORE_VERSION
#define LUMACORE_VERSION "0.8.0"
#endif

namespace lumacore {

inline QString applicationVersion()
{
    return QLatin1String(LUMACORE_VERSION);
}

} // namespace lumacore
