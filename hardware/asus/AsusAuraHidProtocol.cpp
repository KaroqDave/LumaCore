// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/asus/AsusAuraHidProtocol.h"

#include "hardware/common/RgbControllerCatalog.h"

#include <QChar>
#include <QStringList>
#include <QtGlobal>

namespace lumacore::hardware::asus {

namespace {

// --- constants ---

constexpr quint8 kAuraCommandPrefix = 0xEC;
constexpr quint8 kAuraCommandRequestConfig = 0xB0;
constexpr quint8 kAuraCommandConfigResponse = 0x30;
constexpr quint8 kAuraCommandSetMode = 0x35;
constexpr quint8 kAuraCommandSetColors = 0x36;
constexpr quint8 kAuraCommandDirectColors = 0x40;
constexpr quint8 kAuraCommandGen1 = 0x52;
constexpr quint8 kAuraOffMode = 0x00;
constexpr quint8 kAuraStaticMode = 0x01;
constexpr quint8 kAuraBreathingMode = 0x02;
constexpr quint8 kAuraSpectrumCycleMode = 0x04;
constexpr quint8 kAuraRainbowMode = 0x05;
constexpr quint8 kAuraDirectMode = 0xFF;
constexpr int kAuraColorPayloadOffset = 5;
constexpr int kAuraDirectMaxLedsPerPacket = 20;
constexpr int kAuraMaxDirectChannels = 16;
constexpr int kAuraConfigAddressableHeaderOffset = 0x02;
constexpr int kAuraConfigMainboardLedOffset = 0x1B;
constexpr int kAuraConfigRgbHeaderOffset = 0x1D;
constexpr int kAuraConfigTableOffset = 0x04;
constexpr int kAuraConfigTableLength = 60;

int fixedExposedZoneCount(const AsusAuraConfigTable& config);

const QString kOpenRgbProvenance = QStringLiteral(
    "OpenRGB-referenced ASUS Aura USB 65-byte EC35/EC36 mode/color sequence"
);

bool isAsusAuraUsbCatalogEntry(const hardware::common::RgbControllerCatalogEntry& entry)
{
    return QString::fromLatin1(entry.vendorId).compare(QStringLiteral("0B05"), Qt::CaseInsensitive) == 0
        && QString::fromLatin1(entry.family) == QStringLiteral("ASUS Aura USB HID");
}

// --- format helpers ---

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

// --- result helpers ---

AsusAuraHidProtocolResult makeError(const QString& error);

AsusAuraConfigTable invalidConfigTable(const QByteArray& response, const QString& error)
{
    return {false, 0, 0, 0, response, {}, {}, {}, error};
}

AsusAuraHidProtocolResult makeError(const QString& error)
{
    return {false, {}, error};
}

// --- validation ---

AsusAuraHidProtocolResult validateStaticInput(int zoneIndex, int ledCount, int brightness)
{
    if (zoneIndex < 0 || zoneIndex >= kAsusAuraHeaderCount) {
        return {
            false,
            {},
            QStringLiteral("ASUS Aura HID research path only supports headers 0 through %1.").arg(kAsusAuraHeaderCount - 1),
        };
    }

    if (ledCount < 1 || ledCount > kAsusAuraMaxResearchLeds) {
        return {
            false,
            {},
            QStringLiteral("ASUS Aura HID LED count must be between 1 and %1.").arg(kAsusAuraMaxResearchLeds),
        };
    }

    if (brightness < 0 || brightness > 100) {
        return {false, {}, QStringLiteral("ASUS Aura HID brightness must be between 0 and 100.")};
    }

    return {true, {}, {}};
}

AsusAuraHidProtocolResult validateWriteShape(int ledCount, int brightness)
{
    if (ledCount < 1 || ledCount > kAsusAuraMaxResearchLeds) {
        return {
            false,
            {},
            QStringLiteral("ASUS Aura HID LED count must be between 1 and %1.").arg(kAsusAuraMaxResearchLeds),
        };
    }

    if (brightness < 0 || brightness > 100) {
        return {false, {}, QStringLiteral("ASUS Aura HID brightness must be between 0 and 100.")};
    }

    return {true, {}, {}};
}

quint8 scaledChannel(int channel, int brightness)
{
    const double factor = static_cast<double>(brightness) / 100.0;
    return static_cast<quint8>(qRound(static_cast<double>(channel) * factor));
}

// --- report builders ---

QByteArray buildAuraGen1Report()
{
    QByteArray report = auraReport();
    report[0] = static_cast<char>(kAuraCommandPrefix);
    report[1] = static_cast<char>(kAuraCommandGen1);
    report[2] = static_cast<char>(0x53);
    report[3] = '\0';
    report[4] = static_cast<char>(0x01);
    return report;
}

QByteArray buildAuraModeReport(quint8 channel, quint8 mode)
{
    QByteArray report = auraReport();
    report[0] = static_cast<char>(kAuraCommandPrefix);
    report[1] = static_cast<char>(kAuraCommandSetMode);
    report[2] = static_cast<char>(channel);
    report[3] = '\0';
    report[4] = '\0';
    report[5] = static_cast<char>(mode);
    return report;
}

bool appendFixedColorReport(
    QVector<QByteArray>& reports,
    int startLed,
    int ledCount,
    quint8 red,
    quint8 green,
    quint8 blue,
    QString* error
)
{
    if (startLed < 0 || ledCount < 1 || startLed + ledCount > kAsusAuraFixedColorMaskLeds) {
        if (error != nullptr) {
            *error = QStringLiteral("ASUS Aura fixed-channel write target is outside the 16-bit effect-color mask: start=%1 count=%2.")
                         .arg(startLed)
                         .arg(ledCount);
        }
        return false;
    }

    if (kAuraColorPayloadOffset + ((startLed + ledCount) * 3) > kAsusAuraResearchReportLength) {
        if (error != nullptr) {
            *error = QStringLiteral("ASUS Aura fixed-channel color payload does not fit in one 65-byte report: start=%1 count=%2.")
                         .arg(startLed)
                         .arg(ledCount);
        }
        return false;
    }

    const quint16 mask = static_cast<quint16>(((1u << ledCount) - 1u) << startLed);
    QByteArray report = auraReport();
    report[0] = static_cast<char>(kAuraCommandPrefix);
    report[1] = static_cast<char>(kAuraCommandSetColors);
    report[2] = static_cast<char>((mask >> 8) & 0xFF);
    report[3] = static_cast<char>(mask & 0xFF);
    report[4] = '\0';

    for (int ledIndex = 0; ledIndex < ledCount; ++ledIndex) {
        const int offset = kAuraColorPayloadOffset + ((startLed + ledIndex) * 3);
        report[offset] = static_cast<char>(red);
        report[offset + 1] = static_cast<char>(green);
        report[offset + 2] = static_cast<char>(blue);
    }
    reports.append(report);
    return true;
}

bool appendDirectColorReports(
    QVector<QByteArray>& reports,
    int directChannel,
    const QVector<RgbColor>& colors,
    QString* error
)
{
    if (directChannel < 0 || directChannel >= kAuraMaxDirectChannels) {
        if (error != nullptr) {
            *error = QStringLiteral("ASUS Aura direct channel must be between 0 and %1: channel=%2.")
                         .arg(kAuraMaxDirectChannels - 1)
                         .arg(directChannel);
        }
        return false;
    }
    const int ledCount = colors.size();
    if (ledCount < 1 || ledCount > kAsusAuraMaxResearchLeds) {
        if (error != nullptr) {
            *error = QStringLiteral("ASUS Aura direct frame LED count must be between 1 and %1: leds=%2.")
                         .arg(kAsusAuraMaxResearchLeds)
                         .arg(ledCount);
        }
        return false;
    }

    int offset = 0;
    while (offset < ledCount) {
        const int packetLedCount = qMin(kAuraDirectMaxLedsPerPacket, ledCount - offset);
        const bool apply = offset + packetLedCount == ledCount;
        QByteArray report = auraReport();
        report[0] = static_cast<char>(kAuraCommandPrefix);
        report[1] = static_cast<char>(kAuraCommandDirectColors);
        report[2] = static_cast<char>((apply ? 0x80 : 0x00) | directChannel);
        report[3] = static_cast<char>(offset);
        report[4] = static_cast<char>(packetLedCount);

        for (int ledIndex = 0; ledIndex < packetLedCount; ++ledIndex) {
            const int payloadOffset = kAuraColorPayloadOffset + (ledIndex * 3);
            const RgbColor& color = colors.at(offset + ledIndex);
            report[payloadOffset] = static_cast<char>(color.red());
            report[payloadOffset + 1] = static_cast<char>(color.green());
            report[payloadOffset + 2] = static_cast<char>(color.blue());
        }

        reports.append(report);
        offset += packetLedCount;
    }
    return true;
}

bool appendDirectColorReports(
    QVector<QByteArray>& reports,
    int directChannel,
    int ledCount,
    quint8 red,
    quint8 green,
    quint8 blue,
    QString* error
)
{
    QVector<RgbColor> colors;
    colors.fill(RgbColor::fromRgb(red, green, blue), ledCount);
    return appendDirectColorReports(reports, directChannel, colors, error);
}

// --- config-table navigation ---

QString configValidationError(const AsusAuraConfigTable& config)
{
    if (!config.valid) {
        return QStringLiteral("ASUS Aura write requires a verified config table.");
    }
    if (config.channels.isEmpty()) {
        return QStringLiteral("ASUS Aura config table contains no usable channels.");
    }

    int fixedChannels = 0;
    int addressableChannels = 0;
    QVector<int> effectChannels;
    QVector<int> directChannels;
    effectChannels.reserve(config.channels.size());
    directChannels.reserve(config.channels.size());

    for (const AsusAuraConfigChannel& channel : config.channels) {
        if (channel.effectChannel < 0 || channel.effectChannel > 0xFF) {
            return QStringLiteral("ASUS Aura effect channel is outside the byte-sized protocol field: channel=%1.")
                .arg(channel.effectChannel);
        }
        if (effectChannels.contains(channel.effectChannel)) {
            return QStringLiteral("ASUS Aura config table contains duplicate effect channel %1.")
                .arg(channel.effectChannel);
        }
        effectChannels.append(channel.effectChannel);

        if (channel.type == AsusAuraChannelType::Fixed) {
            ++fixedChannels;
            if (fixedChannels > 1) {
                return QStringLiteral("ASUS Aura config table contains multiple fixed channels.");
            }
            if (channel.ledCount < 1 || channel.headerCount < 0
                || channel.headerCount > kAsusAuraHeaderCount
                || channel.headerCount > channel.ledCount) {
                return QStringLiteral(
                    "ASUS Aura fixed-channel geometry is invalid: leds=%1 headers=%2."
                )
                    .arg(channel.ledCount)
                    .arg(channel.headerCount);
            }
            if (channel.headerCount > 0 && channel.ledCount > kAsusAuraFixedColorMaskLeds) {
                return QStringLiteral(
                    "ASUS Aura fixed RGB headers fall outside the 16-bit effect-color mask: leds=%1 headers=%2."
                )
                    .arg(channel.ledCount)
                    .arg(channel.headerCount);
            }
            if (channel.ledCount != config.mainboardLedCount
                || channel.headerCount != config.rgbHeaderCount) {
                return QStringLiteral(
                    "ASUS Aura fixed-channel values are inconsistent with the config table: leds=%1/%2 headers=%3/%4."
                )
                    .arg(channel.ledCount)
                    .arg(config.mainboardLedCount)
                    .arg(channel.headerCount)
                    .arg(config.rgbHeaderCount);
            }
            continue;
        }

        ++addressableChannels;
        if (channel.ledCount < 1 || channel.ledCount > kAsusAuraMaxResearchLeds) {
            return QStringLiteral("ASUS Aura addressable channel LED count is invalid: leds=%1.")
                .arg(channel.ledCount);
        }
        if (channel.directChannel < 0 || channel.directChannel >= kAuraMaxDirectChannels) {
            return QStringLiteral("ASUS Aura direct channel must be between 0 and %1: channel=%2.")
                .arg(kAuraMaxDirectChannels - 1)
                .arg(channel.directChannel);
        }
        if (directChannels.contains(channel.directChannel)) {
            return QStringLiteral("ASUS Aura config table contains duplicate direct channel %1.")
                .arg(channel.directChannel);
        }
        directChannels.append(channel.directChannel);
    }

    if (addressableChannels > kAuraMaxDirectChannels) {
        return QStringLiteral("ASUS Aura config table exceeds the %1 addressable channels encodable by EC40.")
            .arg(kAuraMaxDirectChannels);
    }
    if (addressableChannels != config.addressableHeaderCount) {
        return QStringLiteral(
            "ASUS Aura config table addressable channel count is inconsistent: declared=%1 parsed=%2."
        )
            .arg(config.addressableHeaderCount)
            .arg(addressableChannels);
    }
    if ((fixedChannels == 0) != (config.mainboardLedCount == 0)) {
        return QStringLiteral(
            "ASUS Aura config table fixed-channel presence is inconsistent with mainboard LED count %1."
        )
            .arg(config.mainboardLedCount);
    }
    if (addressableChannels == 0 && fixedExposedZoneCount(config) == 0) {
        if (config.mainboardLedCount > kAsusAuraFixedColorMaskLeds) {
            return QStringLiteral(
                "ASUS Aura mainboard channel spans %1 LEDs, beyond the %2-LED effect-color mask, and no addressable headers are available."
            )
                .arg(config.mainboardLedCount)
                .arg(kAsusAuraFixedColorMaskLeds);
        }
        return QStringLiteral("ASUS Aura config table contains no controllable RGB headers.");
    }

    return {};
}

int fixedChannelIndex(const AsusAuraConfigTable& config)
{
    for (int index = 0; index < config.channels.size(); ++index) {
        if (config.channels.at(index).type == AsusAuraChannelType::Fixed) {
            return index;
        }
    }
    return -1;
}

QVector<int> addressableChannelIndices(const AsusAuraConfigTable& config)
{
    QVector<int> indices;
    for (int index = 0; index < config.channels.size(); ++index) {
        if (config.channels.at(index).type == AsusAuraChannelType::Addressable) {
            indices.append(index);
        }
    }
    return indices;
}

int fixedExposedZoneCount(const AsusAuraConfigTable& config)
{
    const int fixedIndex = fixedChannelIndex(config);
    if (fixedIndex < 0) {
        return 0;
    }

    const AsusAuraConfigChannel& fixed = config.channels.at(fixedIndex);
    const int headers = qBound(0, fixed.headerCount, kAsusAuraHeaderCount);
    if (headers > 0) {
        return headers;
    }
    // The aggregate mainboard zone is written through the fixed-channel
    // effect-color mask, so wider channels cannot be exposed as a zone.
    return fixed.ledCount > 0 && fixed.ledCount <= kAsusAuraFixedColorMaskLeds ? 1 : 0;
}

int exposedZoneCount(const AsusAuraConfigTable& config)
{
    return fixedExposedZoneCount(config) + addressableChannelIndices(config).size();
}

AsusAuraHidProtocolResult validateConfigWriteInput(
    const AsusAuraConfigTable& config,
    int zoneIndex,
    int ledCount,
    int brightness
)
{
    const AsusAuraHidProtocolResult shape = validateWriteShape(ledCount, brightness);
    if (!shape.ok) {
        return shape;
    }

    const QString configError = configValidationError(config);
    if (!configError.isEmpty()) {
        return makeError(configError);
    }

    const int zoneCount = exposedZoneCount(config);
    if (zoneIndex < 0 || zoneIndex >= zoneCount) {
        return makeError(
            QStringLiteral("ASUS Aura zone %1 is not present in config table: exposedZones=%2.")
                .arg(zoneIndex + 1)
                .arg(zoneCount)
        );
    }

    return {true, {}, {}};
}

bool resolveEffectTarget(
    const AsusAuraConfigTable& config,
    int zoneIndex,
    AsusAuraConfigChannel* channel,
    int* colorOffset,
    QString* targetSummary
)
{
    const int fixedIndex = fixedChannelIndex(config);
    const int fixedZoneCount = fixedExposedZoneCount(config);
    if (fixedIndex >= 0 && zoneIndex < fixedZoneCount) {
        const AsusAuraConfigChannel fixed = config.channels.at(fixedIndex);
        const int fixedHeaders = qBound(0, fixed.headerCount, kAsusAuraHeaderCount);
        const int startLed = fixedHeaders > 0
            ? qMax(0, fixed.ledCount - fixedHeaders) + zoneIndex
            : 0;
        if (channel != nullptr) {
            *channel = fixed;
        }
        if (colorOffset != nullptr) {
            *colorOffset = startLed;
        }
        if (targetSummary != nullptr) {
            *targetSummary = fixedHeaders > 0
                ? QStringLiteral("fixedHeader=%1 effectChannel=%2 directChannel=%3 colorOffset=%4")
                      .arg(zoneIndex + 1)
                      .arg(fixed.effectChannel)
                      .arg(fixed.directChannel)
                      .arg(startLed)
                : QStringLiteral("fixedMainboard=true effectChannel=%1 directChannel=%2 colorOffset=%3 leds=%4")
                      .arg(fixed.effectChannel)
                      .arg(fixed.directChannel)
                      .arg(startLed)
                      .arg(fixed.ledCount);
        }
        return true;
    }

    const QVector<int> addressableIndices = addressableChannelIndices(config);
    const int addressableZone = zoneIndex - fixedZoneCount;
    if (addressableZone < 0 || addressableZone >= addressableIndices.size()) {
        return false;
    }

    const AsusAuraConfigChannel addressable = config.channels.at(addressableIndices.at(addressableZone));
    const int fixedColorCount = fixedIndex >= 0 ? config.channels.at(fixedIndex).ledCount : 0;
    const int addressableColorOffset = fixedColorCount + addressableZone;
    if (channel != nullptr) {
        *channel = addressable;
    }
    if (colorOffset != nullptr) {
        *colorOffset = addressableColorOffset;
    }
    if (targetSummary != nullptr) {
        *targetSummary = QStringLiteral("addressableHeader=%1 effectChannel=%2 directChannel=%3 colorOffset=%4")
                             .arg(addressableZone + 1)
                             .arg(addressable.effectChannel)
                             .arg(addressable.directChannel)
                             .arg(addressableColorOffset);
    }
    return true;
}

// --- effect mapping ---

quint8 nativeModeForEffect(RgbEffectType type)
{
    switch (type) {
    case RgbEffectType::Breathing:
        return kAuraBreathingMode;
    case RgbEffectType::ColorCycle:
        return kAuraSpectrumCycleMode;
    case RgbEffectType::Rainbow:
        return kAuraRainbowMode;
    case RgbEffectType::Static:
        return kAuraStaticMode;
    }

    return kAuraStaticMode;
}

bool nativeEffectUsesColor(RgbEffectType type)
{
    return type == RgbEffectType::Breathing;
}

} // namespace

// --- device identity ---

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
    QStringList keys;
    for (const hardware::common::RgbControllerCatalogEntry& entry : hardware::common::kRgbControllerCatalog) {
        if (isAsusAuraUsbCatalogEntry(entry)) {
            keys.append(QStringLiteral("0B05:%1").arg(QString::fromLatin1(entry.productId).toUpper()));
        }
    }
    return keys;
}

bool isAsusAuraUsbVendor(const QString& vendorId)
{
    return vendorId.trimmed().compare(QStringLiteral("0B05"), Qt::CaseInsensitive) == 0;
}

bool isAsusAuraResearchedUsbProduct(const QString& productId)
{
    const QString normalized = productId.trimmed().toUpper();
    const hardware::common::RgbControllerCatalogEntry* entry =
        hardware::common::rgbControllerCatalogEntry(QStringLiteral("0B05"), normalized);
    return entry != nullptr && isAsusAuraUsbCatalogEntry(*entry);
}

bool isAsusAuraWriteValidatedProduct(const QString& productId)
{
    return productId.trimmed().compare(QStringLiteral("19AF"), Qt::CaseInsensitive) == 0;
}

// --- config probe / parse ---

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
        return invalidConfigTable(
            response,
            QStringLiteral("ASUS Aura config response is too short: %1 byte(s).").arg(response.size())
        );
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
        return invalidConfigTable(response, QStringLiteral("ASUS Aura config response did not start with EC 30."));
    }

    const int tableOffset = prefixIndex + kAuraConfigTableOffset;
    if (response.size() < tableOffset + kAuraConfigRgbHeaderOffset + 1) {
        return invalidConfigTable(
            response,
            QStringLiteral("ASUS Aura config response did not include enough config-table bytes.")
        );
    }

    const QByteArray table = response.mid(tableOffset, qMin(kAuraConfigTableLength, response.size() - tableOffset));
    const int addressableHeaders = static_cast<quint8>(table.at(kAuraConfigAddressableHeaderOffset));
    const int mainboardLeds = static_cast<quint8>(table.at(kAuraConfigMainboardLedOffset));
    const int reportedRgbHeaders = static_cast<quint8>(table.at(kAuraConfigRgbHeaderOffset));
    int rgbHeaders = reportedRgbHeaders;
    if (addressableHeaders > kAuraMaxDirectChannels) {
        return invalidConfigTable(
            response,
            QStringLiteral("ASUS Aura config reports %1 addressable headers, exceeding the EC40 channel limit of %2.")
                .arg(addressableHeaders)
                .arg(kAuraMaxDirectChannels)
        );
    }
    QStringList normalizationNotes;
    if (rgbHeaders > mainboardLeds) {
        normalizationNotes.append(
            QStringLiteral("reportedRgbHeaders=%1 exceeds mainboardLeds=%2; fixed RGB headers are not exposed")
                .arg(reportedRgbHeaders)
                .arg(mainboardLeds)
        );
        rgbHeaders = 0;
    }
    if (rgbHeaders > kAsusAuraHeaderCount) {
        return invalidConfigTable(
            response,
            QStringLiteral("ASUS Aura config reports too many fixed RGB headers for the validated path: mainboardLeds=%1 rgbHeaders=%2 maxHeaders=%3.")
                .arg(mainboardLeds)
                .arg(rgbHeaders)
                .arg(kAsusAuraHeaderCount)
        );
    }
    if (rgbHeaders > 0 && mainboardLeds > kAsusAuraFixedColorMaskLeds) {
        return invalidConfigTable(
            response,
            QStringLiteral("ASUS Aura fixed RGB headers fall outside the 16-bit effect-color mask: mainboardLeds=%1 rgbHeaders=%2.")
                .arg(mainboardLeds)
                .arg(rgbHeaders)
        );
    }
    if (rgbHeaders == 0 && mainboardLeds > kAsusAuraFixedColorMaskLeds) {
        normalizationNotes.append(
            QStringLiteral("mainboardLeds=%1 exceeds the %2-LED effect-color mask; the aggregate mainboard zone is not exposed")
                .arg(mainboardLeds)
                .arg(kAsusAuraFixedColorMaskLeds)
        );
    }
    if (rgbHeaders == 0 && addressableHeaders == 0 && mainboardLeds == 0) {
        return invalidConfigTable(response, QStringLiteral("ASUS Aura config response contains no controllable RGB headers."));
    }

    QVector<AsusAuraConfigChannel> channels;
    int effectChannel = 0;
    if (mainboardLeds > 0) {
        channels.append({AsusAuraChannelType::Fixed, effectChannel, 0x04, mainboardLeds, rgbHeaders});
        ++effectChannel;
    }
    for (int index = 0; index < addressableHeaders; ++index) {
        channels.append({AsusAuraChannelType::Addressable, effectChannel, index, kAsusAuraMaxResearchLeds, 0});
        ++effectChannel;
    }

    AsusAuraConfigTable config {
        true,
        addressableHeaders,
        mainboardLeds,
        rgbHeaders,
        response,
        table,
        channels,
        {},
        {},
    };
    const QString validationError = configValidationError(config);
    if (!validationError.isEmpty()) {
        return invalidConfigTable(response, validationError);
    }

    config.summary = QStringLiteral(
        "ASUS Aura config table OK: addressableHeaders=%1 mainboardLeds=%2 rgbHeaders=%3 reportedRgbHeaders=%4 channels=%5 firstBytes=%6%7"
    )
        .arg(addressableHeaders)
        .arg(mainboardLeds)
        .arg(rgbHeaders)
        .arg(reportedRgbHeaders)
        .arg(channels.size())
        .arg(bytesPreview(response))
        .arg(normalizationNotes.isEmpty()
                 ? QString()
                 : QStringLiteral(" notes=%1").arg(normalizationNotes.join(QStringLiteral("; "))));

    return config;
}

bool isAsusAuraConfigTableWriteReady(const AsusAuraConfigTable& config)
{
    return configValidationError(config).isEmpty();
}

int asusAuraFixedExposedZoneCount(const AsusAuraConfigTable& config)
{
    return fixedExposedZoneCount(config);
}

int asusAuraExposedZoneCount(const AsusAuraConfigTable& config)
{
    return exposedZoneCount(config);
}

// --- write serializers ---

AsusAuraHidProtocolResult buildAsusAuraStaticColorPreview(
    int zoneIndex,
    const RgbColor& color,
    int ledCount,
    int brightness
)
{
    const AsusAuraHidProtocolResult validation = validateStaticInput(zoneIndex, ledCount, brightness);
    if (!validation.ok) {
        return validation;
    }

    const quint8 red = scaledChannel(color.red(), brightness);
    const quint8 green = scaledChannel(color.green(), brightness);
    const quint8 blue = scaledChannel(color.blue(), brightness);

    QByteArray report(kAsusAuraResearchReportLength, '\0');
    report[0] = '\0';                 // HID report ID placeholder.
    report[1] = '\x4C';               // LumaCore research marker, not a hardware-approved opcode.
    report[2] = '\x41';               // 'A'
    report[3] = '\x55';               // 'U'
    report[4] = '\x52';               // 'R'
    report[5] = '\x41';               // 'A'
    report[6] = static_cast<char>(qBound(0, ledCount, 255));
    report[7] = static_cast<char>(brightness);
    report[8] = static_cast<char>(red);
    report[9] = static_cast<char>(green);
    report[10] = static_cast<char>(blue);
    report[11] = static_cast<char>(zoneIndex);

    const bool allOffPreview = brightness == 0 || (red == 0 && green == 0 && blue == 0);

    AsusAuraHidPacket packet {
        report,
        {report},
        QStringLiteral(
            "ASUS Aura HID research preview for %1 Header %2: reportLength=%3 leds=%4 brightness=%5 rgb=#%6%7%8 allOffPreview=%9 firstBytes=%10 hardwareWriteApproved=false"
        )
            .arg(asusAuraDeviceKey())
            .arg(zoneIndex + 1)
            .arg(report.size())
            .arg(ledCount)
            .arg(brightness)
            .arg(byteHex(red), byteHex(green), byteHex(blue))
            .arg(allOffPreview ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(bytesPreview(report)),
        QStringLiteral("LumaCore research preview packet; not an ASUS hardware opcode."),
        AsusAuraHidPacketKind::ResearchPreview,
        false,
    };

    return {true, packet, {}};
}

AsusAuraHidProtocolResult buildAsusAuraStaticColorWrite(
    const AsusAuraConfigTable& config,
    int zoneIndex,
    const RgbColor& color,
    int ledCount,
    int brightness
)
{
    const AsusAuraHidProtocolResult validation = validateConfigWriteInput(config, zoneIndex, ledCount, brightness);
    if (!validation.ok) {
        return validation;
    }

    const quint8 red = scaledChannel(color.red(), brightness);
    const quint8 green = scaledChannel(color.green(), brightness);
    const quint8 blue = scaledChannel(color.blue(), brightness);

    AsusAuraConfigChannel target;
    int colorOffset = 0;
    QString targetSummary;
    if (!resolveEffectTarget(config, zoneIndex, &target, &colorOffset, &targetSummary)) {
        return makeError(
            QStringLiteral("ASUS Aura zone %1 is not present in config table: exposedZones=%2.")
                .arg(zoneIndex + 1)
                .arg(exposedZoneCount(config))
        );
    }

    QVector<QByteArray> reports {buildAuraGen1Report()};
    QString error;
    if (target.type == AsusAuraChannelType::Fixed) {
        const int fixedHeaders = qBound(0, target.headerCount, kAsusAuraHeaderCount);
        const int targetLedCount = fixedHeaders > 0 ? 1 : target.ledCount;
        reports.append(buildAuraModeReport(static_cast<quint8>(target.effectChannel), kAuraStaticMode));
        if (!appendFixedColorReport(reports, colorOffset, targetLedCount, red, green, blue, &error)) {
            return makeError(error);
        }
    } else {
        reports.append(buildAuraModeReport(static_cast<quint8>(target.effectChannel), kAuraDirectMode));
        if (!appendDirectColorReports(reports, target.directChannel, ledCount, red, green, blue, &error)) {
            return makeError(error);
        }
    }

    AsusAuraHidPacket packet {
        reports.first(),
        reports,
        QStringLiteral(
            "ASUS Aura HID approved static-color write for %1 Header %2: synchronized=false %3 reportCount=%4 reportLength=%5 leds=%6 brightness=%7 rgb=#%8%9%10 firstReportBytes=%11 hardwareWriteApproved=true provenance=%12"
        )
            .arg(asusAuraDeviceKey())
            .arg(zoneIndex + 1)
            .arg(targetSummary)
            .arg(reports.size())
            .arg(reports.first().size())
            .arg(ledCount)
            .arg(brightness)
            .arg(byteHex(red), byteHex(green), byteHex(blue), bytesPreview(reports.first()), kOpenRgbProvenance),
        kOpenRgbProvenance,
        AsusAuraHidPacketKind::StaticColorWrite,
        true,
    };

    return {true, packet, {}};
}

AsusAuraHidProtocolResult buildAsusAuraNativeEffectWrite(
    const AsusAuraConfigTable& config,
    int zoneIndex,
    const RgbEffect& effect,
    int ledCount
)
{
    if (!effect.isAnimated()) {
        return makeError(QStringLiteral("ASUS Aura native effect write requires an animated LumaCore effect."));
    }

    const AsusAuraHidProtocolResult validation = validateConfigWriteInput(config, zoneIndex, ledCount, effect.brightness());
    if (!validation.ok) {
        return validation;
    }

    AsusAuraConfigChannel target;
    int colorOffset = 0;
    QString targetSummary;
    if (!resolveEffectTarget(config, zoneIndex, &target, &colorOffset, &targetSummary)) {
        return makeError(QStringLiteral("ASUS Aura native effect target could not be resolved."));
    }
    if (target.type == AsusAuraChannelType::Fixed) {
        return makeError(
            QStringLiteral(
                "ASUS Aura native effects are channel-wide and cannot safely target an individual fixed RGB header."
            )
        );
    }
    if (target.type == AsusAuraChannelType::Addressable && nativeEffectUsesColor(effect.type())) {
        return makeError(
            QStringLiteral("ASUS Aura color-bearing native effects on addressable headers are not enabled until EC36 addressable color mapping is verified.")
        );
    }
    if (!nativeEffectUsesColor(effect.type())
        && effect.brightness() != 0
        && effect.brightness() != 100) {
        return makeError(
            QStringLiteral(
                "ASUS Aura native effect brightness must be 0 (off) or 100 because no hardware brightness field is verified."
            )
        );
    }

    const quint8 mode = effect.brightness() == 0
        ? kAuraOffMode
        : nativeModeForEffect(effect.type());
    QVector<QByteArray> reports {
        buildAuraGen1Report(),
        buildAuraModeReport(static_cast<quint8>(target.effectChannel), mode),
    };

    QString colorSummary = QStringLiteral("modeUsesColor=false");
    if (nativeEffectUsesColor(effect.type())) {
        const quint8 red = scaledChannel(effect.color().red(), effect.brightness());
        const quint8 green = scaledChannel(effect.color().green(), effect.brightness());
        const quint8 blue = scaledChannel(effect.color().blue(), effect.brightness());

        QString error;
        if (!appendFixedColorReport(reports, colorOffset, 1, red, green, blue, &error)) {
            return makeError(error);
        }
        colorSummary = QStringLiteral("modeUsesColor=true rgb=#%1%2%3")
                           .arg(byteHex(red), byteHex(green), byteHex(blue));
    }

    const QString modeHex = byteHex(mode);
    AsusAuraHidPacket packet {
        reports.first(),
        reports,
        QStringLiteral(
            "ASUS Aura HID approved native effect write for %1 Header %2: effect=%3 mode=0x%4 %5 %6 reportCount=%7 reportLength=%8 brightness=%9 speed=%10x speedHardwareField=not-documented firstReportBytes=%11 hardwareWriteApproved=true provenance=%12"
        )
            .arg(asusAuraDeviceKey())
            .arg(zoneIndex + 1)
            .arg(rgbEffectTypeToString(effect.type()))
            .arg(modeHex)
            .arg(targetSummary, colorSummary)
            .arg(reports.size())
            .arg(reports.first().size())
            .arg(effect.brightness())
            .arg(effect.speed())
            .arg(bytesPreview(reports.first()), kOpenRgbProvenance),
        kOpenRgbProvenance,
        AsusAuraHidPacketKind::NativeEffectWrite,
        true,
    };

    return {true, packet, {}};
}

AsusAuraHidProtocolResult buildAsusAuraDirectFrameWrite(
    const AsusAuraConfigTable& config,
    int zoneIndex,
    const QVector<RgbColor>& colors,
    bool includeDirectMode
)
{
    const AsusAuraHidProtocolResult validation = validateConfigWriteInput(config, zoneIndex, colors.size(), 100);
    if (!validation.ok) {
        return validation;
    }

    AsusAuraConfigChannel target;
    QString targetSummary;
    if (!resolveEffectTarget(config, zoneIndex, &target, nullptr, &targetSummary)) {
        return makeError(QStringLiteral("ASUS Aura direct-frame target could not be resolved."));
    }
    if (target.type != AsusAuraChannelType::Addressable) {
        return makeError(QStringLiteral("ASUS Aura direct-frame streaming is enabled only for addressable headers."));
    }

    QVector<QByteArray> reports;
    if (includeDirectMode) {
        reports.append(buildAuraGen1Report());
        reports.append(buildAuraModeReport(static_cast<quint8>(target.effectChannel), kAuraDirectMode));
    }

    QString error;
    if (!appendDirectColorReports(reports, target.directChannel, colors, &error)) {
        return makeError(error);
    }

    AsusAuraHidPacket packet {
        reports.first(),
        reports,
        QStringLiteral(
            "ASUS Aura HID approved EC40 host-streamed frame for %1 Header %2: synchronized=false %3 reportCount=%4 reportLength=%5 leds=%6 directModePrimed=%7 speedHardwareField=none firstReportBytes=%8 hardwareWriteApproved=true provenance=%9"
        )
            .arg(asusAuraDeviceKey())
            .arg(zoneIndex + 1)
            .arg(targetSummary)
            .arg(reports.size())
            .arg(reports.first().size())
            .arg(colors.size())
            .arg(includeDirectMode ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(bytesPreview(reports.first()), QStringLiteral("LumaScope-validated ASUS Aura EC40 direct-color stream")),
        QStringLiteral("LumaScope-validated ASUS Aura EC40 direct-color stream"),
        AsusAuraHidPacketKind::DirectFrameWrite,
        true,
    };

    return {true, packet, {}};
}

AsusAuraHidProtocolResult buildAsusAuraAllOffWrite(const AsusAuraConfigTable& config)
{
    const QString configError = configValidationError(config);
    if (!configError.isEmpty()) {
        return makeError(configError);
    }

    QVector<QByteArray> reports {buildAuraGen1Report()};
    for (const AsusAuraConfigChannel& channel : config.channels) {
        reports.append(buildAuraModeReport(static_cast<quint8>(channel.effectChannel), kAuraOffMode));
    }

    AsusAuraHidPacket packet {
        reports.first(),
        reports,
        QStringLiteral(
            "ASUS Aura HID approved all-off write for %1: synchronized=false channels=%2 reportCount=%3 reportLength=%4 mode=off firstBytes=%5 hardwareWriteApproved=true provenance=%6"
        )
            .arg(asusAuraDeviceKey())
            .arg(config.channels.size())
            .arg(reports.size())
            .arg(reports.first().size())
            .arg(bytesPreview(reports.first()), kOpenRgbProvenance),
        kOpenRgbProvenance,
        AsusAuraHidPacketKind::AllOffWrite,
        true,
    };

    return {true, packet, {}};
}

} // namespace lumacore::hardware::asus
