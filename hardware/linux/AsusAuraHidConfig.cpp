#include "hardware/linux/AsusAuraHidProtocol.h"

#include <QChar>
#include <QStringList>
#include <QtGlobal>

namespace lumacore::hardware::linux {

namespace {

constexpr quint8 kAuraCommandPrefix = 0xEC;
constexpr quint8 kAuraCommandRequestConfig = 0xB0;
constexpr quint8 kAuraCommandConfigResponse = 0x30;
constexpr int kAuraConfigAddressableHeaderOffset = 0x02;
constexpr int kAuraConfigMainboardLedOffset = 0x1B;
constexpr int kAuraConfigRgbHeaderOffset = 0x1D;
constexpr int kAuraConfigTableOffset = 0x04;
constexpr int kAuraConfigTableLength = 60;

QString byteHex(quint8 value)
{
    return QStringLiteral("%1").arg(static_cast<unsigned int>(value), 2, 16, QChar('0')).toUpper();
}

QString bytesPreview(const QByteArray& bytes)
{
    QStringList preview;
    const int count = qMin(bytes.size(), 16);
    preview.reserve(count);
    for (int index = 0; index < count; ++index) {
        preview.append(byteHex(static_cast<quint8>(bytes.at(index))));
    }
    return preview.join(QLatin1Char(' '));
}

QByteArray auraReport()
{
    return QByteArray(kAsusAuraResearchReportLength, '\0');
}

} // namespace

QString asusAuraDeviceKey()
{
    return asusAuraDeviceKey(kAsusAuraLedControllerProductId);
}

QString asusAuraDeviceKey(quint16 productId)
{
    return QStringLiteral("0B05:%1")
        .arg(static_cast<unsigned int>(productId), 4, 16, QChar('0'))
        .toUpper();
}

QStringList asusAuraResearchedDeviceKeys()
{
    return {
        asusAuraDeviceKey(kAsusAuraLedControllerProductId),
        asusAuraDeviceKey(kAsusAuraAddressableHeaderProductId),
        asusAuraDeviceKey(kAsusAuraTerminalProductId),
    };
}

bool isAsusAuraUsbVendor(const QString& vendorId)
{
    return vendorId.trimmed().compare(QStringLiteral("0B05"), Qt::CaseInsensitive) == 0;
}

bool isAsusAuraResearchedUsbProduct(const QString& productId)
{
    const QString normalized = productId.trimmed().toUpper();
    return normalized == QStringLiteral("19AF")
        || normalized == QStringLiteral("18F3")
        || normalized == QStringLiteral("1939");
}

bool isAsusAuraWriteValidatedProduct(const QString& productId)
{
    return productId.trimmed().compare(QStringLiteral("19AF"), Qt::CaseInsensitive) == 0;
}

QByteArray buildAsusAuraConfigTableRequest()
{
    QByteArray report = auraReport();
    report[0] = static_cast<char>(kAuraCommandPrefix);
    report[1] = static_cast<char>(kAuraCommandRequestConfig);
    return report;
}

AsusAuraConfigTable parseAsusAuraConfigTableResponse(const QByteArray& response)
{
    if (response.size() < kAsusAuraResearchReportLength) {
        return {
            false,
            0,
            0,
            0,
            response,
            {},
            {},
            {},
            QStringLiteral("ASUS Aura config response is too short: %1 byte(s).").arg(response.size()),
        };
    }

    int prefixIndex = -1;
    if (static_cast<quint8>(response.at(0)) == kAuraCommandPrefix) {
        prefixIndex = 0;
    } else if (response.size() > kAsusAuraResearchReportLength
               && static_cast<quint8>(response.at(0)) == 0
               && static_cast<quint8>(response.at(1)) == kAuraCommandPrefix) {
        prefixIndex = 1;
    }
    if (prefixIndex < 0 || prefixIndex + 1 >= response.size()
        || static_cast<quint8>(response.at(prefixIndex + 1)) != kAuraCommandConfigResponse) {
        return {
            false,
            0,
            0,
            0,
            response,
            {},
            {},
            {},
            QStringLiteral("ASUS Aura config response did not start with EC 30."),
        };
    }

    const int tableOffset = prefixIndex + kAuraConfigTableOffset;
    if (response.size() < tableOffset + kAuraConfigRgbHeaderOffset + 1) {
        return {
            false,
            0,
            0,
            0,
            response,
            {},
            {},
            {},
            QStringLiteral("ASUS Aura config response did not include enough config-table bytes."),
        };
    }

    const QByteArray table = response.mid(tableOffset, qMin(kAuraConfigTableLength, response.size() - tableOffset));
    const int addressableHeaders = static_cast<quint8>(table.at(kAuraConfigAddressableHeaderOffset));
    const int mainboardLeds = static_cast<quint8>(table.at(kAuraConfigMainboardLedOffset));
    int rgbHeaders = static_cast<quint8>(table.at(kAuraConfigRgbHeaderOffset));
    if (mainboardLeds < rgbHeaders) {
        rgbHeaders = 0;
    }

    QVector<AsusAuraConfigChannel> channels;
    int effectChannel = 0;
    if (mainboardLeds > 0) {
        channels.append({AsusAuraChannelType::Fixed, effectChannel, 0x04, mainboardLeds, rgbHeaders});
        ++effectChannel;
    }
    for (int index = 0; index < addressableHeaders; ++index) {
        channels.append({AsusAuraChannelType::Addressable, effectChannel, index, 1, 0});
        ++effectChannel;
    }

    const QString summary = QStringLiteral(
        "ASUS Aura config table OK: addressableHeaders=%1 mainboardLeds=%2 rgbHeaders=%3 channels=%4 firstBytes=%5"
    )
        .arg(addressableHeaders)
        .arg(mainboardLeds)
        .arg(rgbHeaders)
        .arg(channels.size())
        .arg(bytesPreview(response));

    return {true, addressableHeaders, mainboardLeds, rgbHeaders, response, table, channels, summary, {}};
}

} // namespace lumacore::hardware::linux
