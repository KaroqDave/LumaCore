// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/windows/HidapiProbe.h"

#include <QStringList>

#ifdef LUMACORE_HAS_HIDAPI
#include <hidapi/hidapi.h>
#endif

namespace lumacore::hardware::windows {

bool hidapiProbeAvailable()
{
#ifdef LUMACORE_HAS_HIDAPI
    return true;
#else
    return false;
#endif
}

#ifdef LUMACORE_HAS_HIDAPI
namespace {

QString fromWideString(const wchar_t* value)
{
    return value == nullptr ? QString() : QString::fromWCharArray(value).trimmed();
}

QString fromUtf8String(const char* value)
{
    return value == nullptr ? QString() : QString::fromUtf8(value).trimmed();
}

} // namespace
#endif

ProbeResult probeHidapiDevices()
{
    ProbeResult result;
    result.providerName = QStringLiteral("hidapi");
    result.providerAvailable = hidapiProbeAvailable();

#ifndef LUMACORE_HAS_HIDAPI
    result.messages.append(QStringLiteral("hidapi provider is not compiled in."));
    return result;
#else
    if (hid_init() != 0) {
        result.messages.append(QStringLiteral("hidapi initialization failed."));
        return result;
    }

    hid_device_info* head = hid_enumerate(0x0, 0x0);
    for (hid_device_info* current = head; current != nullptr; current = current->next) {
        const QString path = fromUtf8String(current->path);
        const QString manufacturer = fromWideString(current->manufacturer_string);
        const QString product = fromWideString(current->product_string);
        const QString vid = hexWord(static_cast<quint16>(current->vendor_id));
        const QString pid = hexWord(static_cast<quint16>(current->product_id));
        QStringList details;
        details.append(QStringLiteral("VID:PID %1:%2").arg(vid, pid));
        if (!path.isEmpty()) {
            details.append(QStringLiteral("path %1").arg(path));
        }
        if (current->usage_page != 0 || current->usage != 0) {
            details.append(QStringLiteral("usage %1:%2")
                               .arg(hexWord(static_cast<quint16>(current->usage_page)),
                                    hexWord(static_cast<quint16>(current->usage))));
        }
        if (current->interface_number >= 0) {
            details.append(QStringLiteral("interface %1").arg(current->interface_number));
        }

        result.devices.append({
            QStringLiteral("hidapi"),
            stableProbeId(QStringLiteral("winhid"), QStringLiteral("%1-%2-%3").arg(vid, pid, path)),
            product.isEmpty() ? QStringLiteral("Windows HID device %1:%2").arg(vid, pid) : product,
            manufacturer.isEmpty() ? QStringLiteral("HID vendor %1").arg(vid) : manufacturer,
            path,
            details.join(QStringLiteral(", ")),
            vid,
            pid,
            current->interface_number,
            static_cast<quint16>(current->usage_page),
            static_cast<quint16>(current->usage),
            RgbDeviceType::Controller,
        });
    }

    hid_free_enumeration(head);
    hid_exit();
    result.messages.append(QStringLiteral("hidapi discovered %1 Windows HID device(s).").arg(result.devices.size()));
    return result;
#endif
}

} // namespace lumacore::hardware::windows
