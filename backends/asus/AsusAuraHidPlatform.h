// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "hardware/linux/AsusAuraHidProtocol.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include "hardware/windows/HidDeviceWriter.h"
#include "hardware/windows/HidapiProbe.h"
#include "hardware/windows/ProbeResult.h"
#else
#include "hardware/linux/HidDeviceWriter.h"
#include "hardware/linux/HidapiProbe.h"
#include "hardware/linux/ProbeResult.h"
#endif

namespace lumacore::asus_aura_platform {

#ifdef Q_OS_WIN
using ProbeDevice = hardware::windows::ProbeDevice;
using ProbeResult = hardware::windows::ProbeResult;
using DiscoverySupportInfo = hardware::windows::DiscoverySupportInfo;
using HidDeviceWriter = hardware::windows::HidDeviceWriter;
using HidWriteResult = hardware::windows::HidWriteResult;
using HidRequestResult = hardware::windows::HidRequestResult;

inline bool hidapiProbeAvailable()
{
    return hardware::windows::hidapiProbeAvailable();
}

inline ProbeResult probeHidapiDevices()
{
    return hardware::windows::probeHidapiDevices();
}

inline QString stableProbeId(const QString& source, const QString& key)
{
    return hardware::windows::stableProbeId(source, key);
}

inline QString usbVidPidKey(const ProbeDevice& device)
{
    return hardware::windows::usbVidPidKey(device);
}

inline DiscoverySupportInfo discoverySupportInfo(const ProbeDevice& device)
{
    return hardware::windows::discoverySupportInfo(device);
}
#else
using ProbeDevice = hardware::linux::ProbeDevice;
using ProbeResult = hardware::linux::ProbeResult;
using DiscoverySupportInfo = hardware::linux::DiscoverySupportInfo;
using HidDeviceWriter = hardware::linux::HidDeviceWriter;
using HidWriteResult = hardware::linux::HidWriteResult;
using HidRequestResult = hardware::linux::HidRequestResult;

inline bool hidapiProbeAvailable()
{
    return hardware::linux::hidapiProbeAvailable();
}

inline ProbeResult probeHidapiDevices()
{
    return hardware::linux::probeHidapiDevices();
}

inline QString stableProbeId(const QString& source, const QString& key)
{
    return hardware::linux::stableProbeId(source, key);
}

inline QString usbVidPidKey(const ProbeDevice& device)
{
    return hardware::linux::usbVidPidKey(device);
}

inline DiscoverySupportInfo discoverySupportInfo(const ProbeDevice& device)
{
    return hardware::linux::discoverySupportInfo(device);
}
#endif

} // namespace lumacore::asus_aura_platform
