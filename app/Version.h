#pragma once

#include <QString>

#ifndef LUMACORE_VERSION
#define LUMACORE_VERSION "1.1.6.3"
#endif

namespace lumacore {

inline QString applicationVersion()
{
    return QLatin1String(LUMACORE_VERSION);
}

} // namespace lumacore
