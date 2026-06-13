#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace lumacore::hardware::linux {

struct HidWriteResult {
    bool success {false};
    int bytesWritten {0};
    QString error;
};

struct HidRequestResult {
    bool success {false};
    int bytesWritten {0};
    QByteArray response;
    QString error;
};

class HidDeviceWriter
{
public:
    [[nodiscard]] HidWriteResult writeReport(const QString& path, const QByteArray& report) const;
    [[nodiscard]] HidWriteResult writeReports(const QString& path, const QVector<QByteArray>& reports) const;
    [[nodiscard]] HidRequestResult writeReportReadResponse(
        const QString& path,
        const QByteArray& report,
        int expectedResponseLength,
        int timeoutMs = 500
    ) const;
};

} // namespace lumacore::hardware::linux
