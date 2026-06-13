#include "hardware/linux/LibusbProbe.h"

#include <QStringList>

#ifdef LUMACORE_HAS_LIBUSB
#include <libusb-1.0/libusb.h>
#endif

namespace lumacore::hardware::linux {

bool libusbProbeAvailable()
{
#ifdef LUMACORE_HAS_LIBUSB
    return true;
#else
    return false;
#endif
}

ProbeResult probeLibusbDevices()
{
    ProbeResult result;
    result.providerName = QStringLiteral("libusb");
    result.providerAvailable = libusbProbeAvailable();

#ifndef LUMACORE_HAS_LIBUSB
    result.messages.append(QStringLiteral("libusb provider is not compiled in."));
    return result;
#else
    libusb_context* context = nullptr;
    const int initStatus = libusb_init(&context);
    if (initStatus != 0) {
        result.messages.append(QStringLiteral("libusb initialization failed: %1").arg(initStatus));
        return result;
    }

    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(context, &list);
    if (count < 0) {
        result.messages.append(QStringLiteral("libusb device list failed: %1").arg(static_cast<qlonglong>(count)));
        libusb_exit(context);
        return result;
    }

    for (ssize_t index = 0; index < count; ++index) {
        libusb_device* device = list[index];
        if (device == nullptr) {
            continue;
        }

        libusb_device_descriptor descriptor {};
        if (libusb_get_device_descriptor(device, &descriptor) != 0) {
            continue;
        }

        const QString vid = hexWord(descriptor.idVendor);
        const QString pid = hexWord(descriptor.idProduct);
        const quint8 bus = libusb_get_bus_number(device);
        const quint8 address = libusb_get_device_address(device);
        QStringList details;
        details.append(QStringLiteral("VID:PID %1:%2").arg(vid, pid));
        details.append(QStringLiteral("bus %1 address %2").arg(bus).arg(address));
        details.append(QStringLiteral("class 0x%1 subclass 0x%2 protocol 0x%3")
                           .arg(descriptor.bDeviceClass, 2, 16, QLatin1Char('0'))
                           .arg(descriptor.bDeviceSubClass, 2, 16, QLatin1Char('0'))
                           .arg(descriptor.bDeviceProtocol, 2, 16, QLatin1Char('0'))
                           .toUpper());

        const QString path = QStringLiteral("usb:%1:%2").arg(bus).arg(address);
        result.devices.append({
            QStringLiteral("libusb"),
            stableProbeId(QStringLiteral("usb"), QStringLiteral("%1-%2-%3-%4").arg(vid, pid).arg(bus).arg(address)),
            QStringLiteral("USB device %1:%2").arg(vid, pid),
            QStringLiteral("USB vendor %1").arg(vid),
            path,
            details.join(QStringLiteral(", ")),
            vid,
            pid,
            -1,
            0,
            0,
            RgbDeviceType::Controller,
        });
    }

    libusb_free_device_list(list, 1);
    libusb_exit(context);
    result.messages.append(QStringLiteral("libusb discovered %1 device(s).").arg(result.devices.size()));
    return result;
#endif
}

} // namespace lumacore::hardware::linux
