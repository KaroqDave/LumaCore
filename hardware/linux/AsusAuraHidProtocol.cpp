#include "hardware/linux/AsusAuraHidProtocol.h"

#include <QChar>
#include <QStringList>
#include <QtGlobal>

namespace lumacore::hardware::linux {

namespace {

constexpr quint8 kAuraCommandPrefix = 0xEC;
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
constexpr quint8 kAuraSyncChannel = 0x00;
constexpr quint8 kAuraAllLedMask = 0xFF;
constexpr int kAuraColorPayloadOffset = 5;
constexpr int kAuraDirectMaxLedsPerPacket = 20;

const QString kOpenRgbProvenance = QStringLiteral(
    "OpenRGB-referenced ASUS Aura USB 65-byte EC35/EC36 mode/color sequence"
);

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

AsusAuraHidProtocolResult makeError(const QString& error);

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

QByteArray auraReport()
{
    return QByteArray(kAsusAuraResearchReportLength, '\0');
}

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

AsusAuraHidProtocolResult makeError(const QString& error)
{
    return {false, {}, error};
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
    if (startLed < 0 || ledCount < 1 || startLed + ledCount > 16) {
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

void appendDirectColorReports(
    QVector<QByteArray>& reports,
    int directChannel,
    int ledCount,
    quint8 red,
    quint8 green,
    quint8 blue
)
{
    int offset = 0;
    while (offset < ledCount) {
        const int packetLedCount = qMin(kAuraDirectMaxLedsPerPacket, ledCount - offset);
        const bool apply = offset + packetLedCount == ledCount;
        QByteArray report = auraReport();
        report[0] = static_cast<char>(kAuraCommandPrefix);
        report[1] = static_cast<char>(kAuraCommandDirectColors);
        report[2] = static_cast<char>((apply ? 0x80 : 0x00) | (directChannel & 0x0F));
        report[3] = static_cast<char>(offset);
        report[4] = static_cast<char>(packetLedCount);

        for (int ledIndex = 0; ledIndex < packetLedCount; ++ledIndex) {
            const int payloadOffset = kAuraColorPayloadOffset + (ledIndex * 3);
            report[payloadOffset] = static_cast<char>(red);
            report[payloadOffset + 1] = static_cast<char>(green);
            report[payloadOffset + 2] = static_cast<char>(blue);
        }

        reports.append(report);
        offset += packetLedCount;
    }
}

AsusAuraConfigTable fallbackSynchronizedConfig()
{
    AsusAuraConfigTable config;
    config.valid = true;
    config.mainboardLedCount = 8;
    config.rgbHeaderCount = 1;
    config.channels.append({AsusAuraChannelType::Fixed, 0, 0x04, 8, 1});
    config.summary = QStringLiteral("Fallback synchronized ASUS Aura config for serializer tests.");
    return config;
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

int fixedExposedHeaderCount(const AsusAuraConfigTable& config)
{
    const int fixedIndex = fixedChannelIndex(config);
    if (fixedIndex < 0) {
        return 0;
    }

    return qBound(0, config.channels.at(fixedIndex).headerCount, kAsusAuraHeaderCount);
}

int exposedZoneCount(const AsusAuraConfigTable& config)
{
    return fixedExposedHeaderCount(config) + addressableChannelIndices(config).size();
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

    if (!config.valid || config.channels.isEmpty()) {
        return makeError(QStringLiteral("ASUS Aura write requires a verified config table."));
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
    const int fixedHeaderCount = fixedExposedHeaderCount(config);
    if (fixedIndex >= 0 && zoneIndex < fixedHeaderCount) {
        const AsusAuraConfigChannel fixed = config.channels.at(fixedIndex);
        const int startLed = qMax(0, fixed.ledCount - fixed.headerCount) + zoneIndex;
        if (channel != nullptr) {
            *channel = fixed;
        }
        if (colorOffset != nullptr) {
            *colorOffset = startLed;
        }
        if (targetSummary != nullptr) {
            *targetSummary = QStringLiteral("fixedHeader=%1 effectChannel=%2 directChannel=%3 colorOffset=%4")
                                 .arg(zoneIndex + 1)
                                 .arg(fixed.effectChannel)
                                 .arg(fixed.directChannel)
                                 .arg(startLed);
        }
        return true;
    }

    const QVector<int> addressableIndices = addressableChannelIndices(config);
    const int addressableZone = zoneIndex - fixedHeaderCount;
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

int startLedForChannel(const AsusAuraConfigTable& config, int channelIndex)
{
    int startLed = 0;
    for (int index = 0; index < channelIndex && index < config.channels.size(); ++index) {
        startLed += config.channels.at(index).ledCount;
    }
    return startLed;
}

} // namespace

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
    int zoneIndex,
    const RgbColor& color,
    int ledCount,
    int brightness
)
{
    return buildAsusAuraStaticColorWrite(fallbackSynchronizedConfig(), zoneIndex, color, ledCount, brightness);
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

    QVector<QByteArray> reports {buildAuraGen1Report()};
    QString targetSummary;
    const int fixedIndex = fixedChannelIndex(config);
    const int fixedHeaderCount = fixedExposedHeaderCount(config);
    if (fixedIndex >= 0 && zoneIndex < fixedHeaderCount) {
        const AsusAuraConfigChannel fixed = config.channels.at(fixedIndex);
        const int startLed = qMax(0, fixed.ledCount - fixed.headerCount) + zoneIndex;
        reports.append(buildAuraModeReport(static_cast<quint8>(fixed.effectChannel), kAuraStaticMode));

        QString error;
        if (!appendFixedColorReport(reports, startLed, 1, red, green, blue, &error)) {
            return makeError(error);
        }
        targetSummary = QStringLiteral("fixedHeader=%1 effectChannel=%2 directChannel=%3 startLed=%4")
                            .arg(zoneIndex + 1)
                            .arg(fixed.effectChannel)
                            .arg(fixed.directChannel)
                            .arg(startLed);
    } else {
        const QVector<int> addressableIndices = addressableChannelIndices(config);
        const int addressableZone = zoneIndex - fixedHeaderCount;
        if (addressableZone < 0 || addressableZone >= addressableIndices.size()) {
            return makeError(
                QStringLiteral("ASUS Aura zone %1 is not present in config table: fixedRgbHeaders=%2 addressableHeaders=%3.")
                    .arg(zoneIndex + 1)
                    .arg(fixedHeaderCount)
                    .arg(addressableIndices.size())
            );
        }

        const int channelIndex = addressableIndices.at(addressableZone);
        const AsusAuraConfigChannel addressable = config.channels.at(channelIndex);
        reports.append(buildAuraModeReport(static_cast<quint8>(addressable.effectChannel), kAuraDirectMode));
        appendDirectColorReports(reports, addressable.directChannel, ledCount, red, green, blue);
        targetSummary = QStringLiteral("addressableHeader=%1 effectChannel=%2 directChannel=%3 directLeds=%4")
                            .arg(addressableZone + 1)
                            .arg(addressable.effectChannel)
                            .arg(addressable.directChannel)
                            .arg(ledCount);
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
    if (target.type == AsusAuraChannelType::Addressable && nativeEffectUsesColor(effect.type())) {
        return makeError(
            QStringLiteral("ASUS Aura color-bearing native effects on addressable headers are not enabled until EC36 addressable color mapping is verified.")
        );
    }

    const quint8 mode = nativeModeForEffect(effect.type());
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

AsusAuraHidProtocolResult buildAsusAuraAllOffWrite()
{
    return buildAsusAuraAllOffWrite(fallbackSynchronizedConfig());
}

AsusAuraHidProtocolResult buildAsusAuraAllOffWrite(const AsusAuraConfigTable& config)
{
    if (!config.valid || config.channels.isEmpty()) {
        return makeError(QStringLiteral("ASUS Aura all-off requires a verified config table."));
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

} // namespace lumacore::hardware::linux
