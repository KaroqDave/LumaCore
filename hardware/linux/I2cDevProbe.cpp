// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/linux/I2cDevProbe.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#ifdef LUMACORE_HAS_I2C_DEV
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace lumacore::hardware::linux {

namespace {

QString readTrimmedFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

} // namespace

bool i2cDevProbeAvailable()
{
#ifdef LUMACORE_HAS_I2C_DEV
    return true;
#else
    return false;
#endif
}

ProbeResult probeI2cDevAdapters()
{
    ProbeResult result;
    result.providerName = QStringLiteral("i2c-dev");
    result.providerAvailable = i2cDevProbeAvailable();

#ifndef LUMACORE_HAS_I2C_DEV
    result.messages.append(QStringLiteral("i2c-dev provider is not compiled in."));
    return result;
#else
    const QDir i2cClassDir(QStringLiteral("/sys/class/i2c-dev"));
    const QStringList entries = i2cClassDir.entryList({QStringLiteral("i2c-*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries) {
        const QString adapterNumber = entry.mid(QStringLiteral("i2c-").size());
        const QString sysPath = i2cClassDir.filePath(entry);
        const QString devPath = QStringLiteral("/dev/%1").arg(entry);
        const QString adapterName = readTrimmedFile(QDir(sysPath).filePath(QStringLiteral("name")));

        QStringList details;
        details.append(QStringLiteral("adapter %1").arg(adapterNumber));
        if (!adapterName.isEmpty()) {
            details.append(QStringLiteral("name %1").arg(adapterName));
        }

        if (QFileInfo::exists(devPath)) {
            details.append(QStringLiteral("device node %1").arg(devPath));
            const int fd = ::open(qPrintable(devPath), O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                unsigned long funcs = 0;
                if (::ioctl(fd, I2C_FUNCS, &funcs) == 0) {
                    details.append(QStringLiteral("funcs 0x%1").arg(funcs, 0, 16).toUpper());
                } else {
                    details.append(QStringLiteral("funcs unavailable"));
                }
                ::close(fd);
            } else {
                details.append(QStringLiteral("read-only open denied"));
            }
        } else {
            details.append(QStringLiteral("device node missing"));
        }

        result.devices.append({
            QStringLiteral("i2c-dev"),
            stableProbeId(QStringLiteral("i2c"), adapterNumber),
            adapterName.isEmpty() ? QStringLiteral("i2c adapter %1").arg(adapterNumber) : adapterName,
            QStringLiteral("Linux i2c-dev"),
            devPath,
            details.join(QStringLiteral(", ")),
            {},
            {},
            -1,
            0,
            0,
            RgbDeviceType::Controller,
        });
    }

    result.messages.append(QStringLiteral("i2c-dev discovered %1 adapter(s).").arg(result.devices.size()));
    return result;
#endif
}

} // namespace lumacore::hardware::linux
