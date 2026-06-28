// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/windows/HidDeviceWriter.h"

#include <QtGlobal>

#ifdef LUMACORE_HAS_HIDAPI
#include <hidapi/hidapi.h>
#endif

namespace lumacore::hardware::windows {

namespace {

QString validatePathAndReports(const QString& path, const QVector<QByteArray>& reports)
{
    if (path.trimmed().isEmpty()) {
        return QStringLiteral("HID path is empty.");
    }

    if (reports.isEmpty()) {
        return QStringLiteral("HID report sequence is empty.");
    }

    for (int index = 0; index < reports.size(); ++index) {
        if (reports.at(index).isEmpty()) {
            return QStringLiteral("HID report %1 is empty.").arg(index + 1);
        }
    }

    return {};
}

} // namespace

HidWriteResult HidDeviceWriter::writeReport(const QString& path, const QByteArray& report) const
{
    return writeReports(path, {report});
}

HidWriteResult HidDeviceWriter::writeReports(const QString& path, const QVector<QByteArray>& reports) const
{
    const QString validationError = validatePathAndReports(path, reports);
    if (!validationError.isEmpty()) {
        return {false, 0, validationError};
    }

#ifndef LUMACORE_HAS_HIDAPI
    Q_UNUSED(path)
    Q_UNUSED(reports)
    return {false, 0, QStringLiteral("hidapi writer is not compiled in.")};
#else
    if (hid_init() != 0) {
        return {false, 0, QStringLiteral("hidapi initialization failed.")};
    }

    hid_device* device = hid_open_path(path.toUtf8().constData());
    if (device == nullptr) {
        hid_exit();
        return {false, 0, QStringLiteral("Could not open Windows HID device path: %1").arg(path)};
    }

    int totalWritten = 0;
    for (int index = 0; index < reports.size(); ++index) {
        const QByteArray& report = reports.at(index);
        const int written = hid_write(
            device,
            reinterpret_cast<const unsigned char*>(report.constData()),
            static_cast<std::size_t>(report.size())
        );
        if (written < 0) {
            hid_close(device);
            hid_exit();
            return {
                false,
                totalWritten,
                QStringLiteral("Windows hid_write failed for %1 on report %2.").arg(path).arg(index + 1),
            };
        }

        if (written != report.size()) {
            hid_close(device);
            hid_exit();
            return {
                false,
                totalWritten + qMax(0, written),
                QStringLiteral("Short Windows HID write for %1 on report %2: wrote %3 of %4 byte(s).")
                    .arg(path)
                    .arg(index + 1)
                    .arg(written)
                    .arg(report.size()),
            };
        }

        totalWritten += written;
    }

    hid_close(device);
    hid_exit();

    return {true, totalWritten, {}};
#endif
}

HidRequestResult HidDeviceWriter::writeReportReadResponse(
    const QString& path,
    const QByteArray& report,
    int expectedResponseLength,
    int timeoutMs
) const
{
    const QString validationError = validatePathAndReports(path, {report});
    if (!validationError.isEmpty()) {
        return {false, 0, {}, validationError};
    }

    if (expectedResponseLength < 1) {
        return {false, 0, {}, QStringLiteral("Expected HID response length must be positive.")};
    }

#ifndef LUMACORE_HAS_HIDAPI
    Q_UNUSED(path)
    Q_UNUSED(report)
    Q_UNUSED(expectedResponseLength)
    Q_UNUSED(timeoutMs)
    return {false, 0, {}, QStringLiteral("hidapi writer is not compiled in.")};
#else
    if (hid_init() != 0) {
        return {false, 0, {}, QStringLiteral("hidapi initialization failed.")};
    }

    hid_device* device = hid_open_path(path.toUtf8().constData());
    if (device == nullptr) {
        hid_exit();
        return {false, 0, {}, QStringLiteral("Could not open Windows HID device path: %1").arg(path)};
    }

    const int written = hid_write(
        device,
        reinterpret_cast<const unsigned char*>(report.constData()),
        static_cast<std::size_t>(report.size())
    );
    if (written < 0) {
        hid_close(device);
        hid_exit();
        return {false, 0, {}, QStringLiteral("Windows hid_write failed for request on %1.").arg(path)};
    }
    if (written != report.size()) {
        hid_close(device);
        hid_exit();
        return {
            false,
            qMax(0, written),
            {},
            QStringLiteral("Short Windows HID request write for %1: wrote %2 of %3 byte(s).")
                .arg(path)
                .arg(written)
                .arg(report.size()),
        };
    }

    QByteArray response(expectedResponseLength, '\0');
    const int read = hid_read_timeout(
        device,
        reinterpret_cast<unsigned char*>(response.data()),
        static_cast<std::size_t>(response.size()),
        timeoutMs
    );
    hid_close(device);
    hid_exit();

    if (read < 0) {
        return {false, written, {}, QStringLiteral("Windows hid_read_timeout failed for %1.").arg(path)};
    }
    if (read == 0) {
        return {false, written, {}, QStringLiteral("Timed out waiting for Windows HID response from %1.").arg(path)};
    }

    response.truncate(read);
    return {true, written, response, {}};
#endif
}

} // namespace lumacore::hardware::windows
