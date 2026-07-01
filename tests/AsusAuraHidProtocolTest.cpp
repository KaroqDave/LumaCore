// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/linux/AsusAuraHidProtocol.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QStringList>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    using namespace lumacore::hardware::linux;

    if (!require(asusAuraDeviceKey() == QStringLiteral("0B05:19AF"), "default ASUS Aura key should stay on the validated write target")) {
        return 1;
    }
    if (!require(isAsusAuraUsbVendor(QStringLiteral("0b05")), "ASUS Aura vendor matching should be case-insensitive")) {
        return 1;
    }
    if (!require(isAsusAuraWriteValidatedProduct(QStringLiteral("19af")), "19AF should be the only validated write product")) {
        return 1;
    }
    if (!require(!isAsusAuraWriteValidatedProduct(QStringLiteral("18F3")), "researched adjacent PIDs must not be write-validated")) {
        return 1;
    }
    if (!require(asusAuraResearchedDeviceKeys().contains(QStringLiteral("0B05:18F3")), "researched ASUS Aura keys should include adjacent documented PIDs")) {
        return 1;
    }
    const QStringList researchedProductIds {
        QStringLiteral("19AF"),
        QStringLiteral("18F3"),
        QStringLiteral("1939"),
        QStringLiteral("1867"),
        QStringLiteral("1872"),
        QStringLiteral("18A3"),
        QStringLiteral("18A5"),
        QStringLiteral("1AA6"),
        QStringLiteral("1889"),
    };
    const QStringList researchedDeviceKeys = asusAuraResearchedDeviceKeys();
    for (const QString& productId : researchedProductIds) {
        if (!require(
                isAsusAuraResearchedUsbProduct(productId),
                "cataloged ASUS Aura PIDs should be treated as researched"
            )
            || !require(
                researchedDeviceKeys.contains(QStringLiteral("0B05:%1").arg(productId)),
                "researched ASUS Aura device key list should include every cataloged PID"
            )
            || !require(
                isAsusAuraWriteValidatedProduct(productId) == (productId == QStringLiteral("19AF")),
                "only 0B05:19AF should be write-validated"
            )) {
            return 1;
        }
    }

    const QByteArray configRequest = buildAsusAuraConfigTableRequest();
    if (!require(configRequest.size() == kAsusAuraResearchReportLength, "config request should be 65 bytes")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(configRequest.at(0)) == 0xEC, "config request should use Aura command prefix")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(configRequest.at(1)) == 0xB0, "config request should use ECB0 command")) {
        return 1;
    }

    QByteArray configResponse(kAsusAuraResearchReportLength, '\0');
    configResponse[0] = static_cast<char>(0xEC);
    configResponse[1] = static_cast<char>(0x30);
    configResponse[0x04 + 0x02] = static_cast<char>(2);
    configResponse[0x04 + 0x1B] = static_cast<char>(3);
    configResponse[0x04 + 0x1D] = static_cast<char>(1);
    const AsusAuraConfigTable parsedConfig = parseAsusAuraConfigTableResponse(configResponse);
    if (!require(parsedConfig.valid, "EC30 config response should parse")) {
        return 1;
    }
    if (!require(isAsusAuraConfigTableWriteReady(parsedConfig), "parsed config should be write-ready")) {
        return 1;
    }
    if (!require(parsedConfig.addressableHeaderCount == 2, "config parser should extract addressable header count")) {
        return 1;
    }
    if (!require(parsedConfig.mainboardLedCount == 3, "config parser should extract mainboard LED count")) {
        return 1;
    }
    if (!require(parsedConfig.rgbHeaderCount == 1, "config parser should extract RGB header count")) {
        return 1;
    }
    if (!require(parsedConfig.channels.size() == 3, "config parser should build fixed plus addressable channels")) {
        return 1;
    }
    if (!require(!parseAsusAuraConfigTableResponse(QByteArray(8, '\0')).valid, "short config responses should be rejected")) {
        return 1;
    }
    QByteArray reportIdConfigResponse(kAsusAuraResearchReportLength + 1, '\0');
    reportIdConfigResponse[1] = static_cast<char>(0xEC);
    reportIdConfigResponse[2] = static_cast<char>(0x30);
    reportIdConfigResponse[1 + 0x04 + 0x02] = static_cast<char>(1);
    reportIdConfigResponse[1 + 0x04 + 0x1B] = static_cast<char>(4);
    reportIdConfigResponse[1 + 0x04 + 0x1D] = static_cast<char>(1);
    const AsusAuraConfigTable reportIdParsedConfig = parseAsusAuraConfigTableResponse(reportIdConfigResponse);
    if (!require(reportIdParsedConfig.valid, "EC30 config responses with a leading report ID should parse")) {
        return 1;
    }
    if (!require(reportIdParsedConfig.addressableHeaderCount == 1, "report-ID config parser should extract addressable headers")) {
        return 1;
    }
    QByteArray invalidConfigResponse(kAsusAuraResearchReportLength, '\0');
    invalidConfigResponse[0] = static_cast<char>(0xEC);
    invalidConfigResponse[1] = static_cast<char>(0x31);
    if (!require(!parseAsusAuraConfigTableResponse(invalidConfigResponse).valid, "non-EC30 config responses should be rejected")) {
        return 1;
    }
    QByteArray inconsistentConfigResponse(kAsusAuraResearchReportLength, '\0');
    inconsistentConfigResponse[0] = static_cast<char>(0xEC);
    inconsistentConfigResponse[1] = static_cast<char>(0x30);
    inconsistentConfigResponse[0x04 + 0x1B] = static_cast<char>(1);
    inconsistentConfigResponse[0x04 + 0x1D] = static_cast<char>(3);
    const AsusAuraConfigTable inconsistentConfig = parseAsusAuraConfigTableResponse(inconsistentConfigResponse);
    if (!require(!inconsistentConfig.valid, "config responses with inconsistent RGB header counts should be rejected")) {
        return 1;
    }
    if (!require(
            inconsistentConfig.error.contains(QStringLiteral("invalid fixed-channel geometry")),
            "inconsistent RGB header counts should report invalid geometry"
        )) {
        return 1;
    }
    QByteArray emptyConfigResponse(kAsusAuraResearchReportLength, '\0');
    emptyConfigResponse[0] = static_cast<char>(0xEC);
    emptyConfigResponse[1] = static_cast<char>(0x30);
    if (!require(!parseAsusAuraConfigTableResponse(emptyConfigResponse).valid, "empty EC30 config tables should be rejected")) {
        return 1;
    }
    QByteArray oversizedAddressableConfigResponse(kAsusAuraResearchReportLength, '\0');
    oversizedAddressableConfigResponse[0] = static_cast<char>(0xEC);
    oversizedAddressableConfigResponse[1] = static_cast<char>(0x30);
    oversizedAddressableConfigResponse[0x04 + 0x02] = static_cast<char>(17);
    if (!require(
            !parseAsusAuraConfigTableResponse(oversizedAddressableConfigResponse).valid,
            "addressable channel counts beyond the EC40 field should be rejected"
        )) {
        return 1;
    }
    AsusAuraConfigTable emptyMarkedValidConfig;
    emptyMarkedValidConfig.valid = true;
    if (!require(
            !isAsusAuraConfigTableWriteReady(emptyMarkedValidConfig),
            "valid flags without usable channels must not advertise write readiness"
        )) {
        return 1;
    }

    const AsusAuraHidProtocolResult valid = buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(20, 40, 80), 1, 25);
    if (!require(valid.ok, "valid static color packet should build")) {
        return 1;
    }
    if (!require(valid.packet.report.size() == kAsusAuraResearchReportLength, "report length should match research length")) {
        return 1;
    }
    if (!require(!valid.packet.hardwareWriteApproved, "research packet must not be hardware-approved")) {
        return 1;
    }
    if (!require(valid.packet.kind == AsusAuraHidPacketKind::ResearchPreview, "preview packet should be marked as research")) {
        return 1;
    }
    if (!require(valid.packet.reports.size() == 1, "preview packet should expose one report")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(valid.packet.report.at(8)) == 5, "red channel should be brightness-scaled")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(valid.packet.report.at(9)) == 10, "green channel should be brightness-scaled")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(valid.packet.report.at(10)) == 20, "blue channel should be brightness-scaled")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(valid.packet.report.at(11)) == 0, "header 1 should encode zone index 0")) {
        return 1;
    }
    if (!require(valid.packet.summary.contains(QStringLiteral("Header 1")), "summary should include the header label")) {
        return 1;
    }

    const AsusAuraHidProtocolResult header3 = buildAsusAuraStaticColorPreview(2, lumacore::RgbColor(1, 2, 3), 30, 100);
    if (!require(header3.ok, "header 3 preview should build")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(header3.packet.report.at(11)) == 2, "header 3 should encode zone index 2")) {
        return 1;
    }
    if (!require(header3.packet.summary.contains(QStringLiteral("Header 3")), "header 3 summary should include the header label")) {
        return 1;
    }

    const AsusAuraHidProtocolResult allOff = buildAsusAuraStaticColorPreview(1, lumacore::RgbColor(0, 0, 0), 30, 100);
    if (!require(allOff.ok, "all-off static preview should build")) {
        return 1;
    }
    if (!require(allOff.packet.summary.contains(QStringLiteral("allOffPreview=true")), "all-off preview should be marked in the summary")) {
        return 1;
    }

    const AsusAuraHidProtocolResult staticWrite = buildAsusAuraStaticColorWrite(parsedConfig, 0, lumacore::RgbColor(20, 40, 80), 10, 25);
    if (!require(staticWrite.ok, "approved static color write should build")) {
        return 1;
    }
    if (!require(staticWrite.packet.hardwareWriteApproved, "approved static color write should be hardware-approved")) {
        return 1;
    }
    if (!require(staticWrite.packet.kind == AsusAuraHidPacketKind::StaticColorWrite, "static write should be marked as a hardware write")) {
        return 1;
    }
    if (!require(staticWrite.packet.reports.size() == 3, "fixed static write should contain gen1, mode, and color reports")) {
        return 1;
    }
    const QByteArray gen1Report = staticWrite.packet.reports.at(0);
    const QByteArray modeReport = staticWrite.packet.reports.at(1);
    const QByteArray colorReport = staticWrite.packet.reports.at(2);
    if (!require(static_cast<unsigned char>(gen1Report.at(0)) == 0xEC, "gen1 report should use Aura command prefix")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(gen1Report.at(1)) == 0x52, "gen1 report should use EC52 command")) {
        return 1;
    }
    if (!require(modeReport.size() == kAsusAuraResearchReportLength, "mode report should be 65 bytes")) {
        return 1;
    }
    if (!require(colorReport.size() == kAsusAuraResearchReportLength, "color report should be 65 bytes")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(modeReport.at(0)) == 0xEC, "mode report should use Aura command prefix")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(modeReport.at(1)) == 0x35, "mode report should use EC35 set-mode command")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(modeReport.at(5)) == 0x01, "mode report should request static mode")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorReport.at(0)) == 0xEC, "color report should use Aura command prefix")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorReport.at(1)) == 0x36, "color report should use EC36 set-colors command")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorReport.at(2)) == 0x00, "color report high mask should match fixed RGB header target")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorReport.at(3)) == 0x04, "color report low mask should match fixed RGB header target")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorReport.at(11)) == 5, "approved red channel should be brightness-scaled at fixed header offset")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorReport.at(12)) == 10, "approved green channel should be brightness-scaled at fixed header offset")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorReport.at(13)) == 20, "approved blue channel should be brightness-scaled at fixed header offset")) {
        return 1;
    }
    if (!require(staticWrite.packet.provenance.contains(QStringLiteral("OpenRGB")), "approved write should carry provenance")) {
        return 1;
    }

    const AsusAuraHidProtocolResult addressableWrite = buildAsusAuraStaticColorWrite(parsedConfig, 1, lumacore::RgbColor(20, 40, 80), 30, 25);
    if (!require(addressableWrite.ok, "addressable static write should build")) {
        return 1;
    }
    if (!require(addressableWrite.packet.reports.size() == 4, "addressable write should contain gen1, direct-mode, and chunked direct color reports")) {
        return 1;
    }
    const QByteArray directModeReport = addressableWrite.packet.reports.at(1);
    const QByteArray firstDirectReport = addressableWrite.packet.reports.at(2);
    const QByteArray finalDirectReport = addressableWrite.packet.reports.at(3);
    if (!require(static_cast<unsigned char>(directModeReport.at(5)) == 0xFF, "addressable mode report should request direct mode")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(firstDirectReport.at(1)) == 0x40, "addressable color report should use EC40 direct command")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(firstDirectReport.at(2)) == 0x00, "first direct report should not set apply bit")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(finalDirectReport.at(2)) == 0x80, "final direct report should set apply bit")) {
        return 1;
    }
    AsusAuraConfigTable invalidDirectChannelConfig = parsedConfig;
    invalidDirectChannelConfig.channels[1].directChannel = 16;
    if (!require(
            !buildAsusAuraStaticColorWrite(
                invalidDirectChannelConfig,
                1,
                lumacore::RgbColor(20, 40, 80),
                30,
                25
            ).ok,
            "out-of-range direct channels should be rejected instead of aliased"
        )) {
        return 1;
    }

    const AsusAuraHidProtocolResult breathingWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        0,
        lumacore::RgbEffect(lumacore::RgbEffectType::Breathing, lumacore::RgbColor(20, 40, 80), 1.5, 25),
        1
    );
    if (!require(!breathingWrite.ok, "fixed-header native effects should be rejected because their effect channel is shared")) {
        return 1;
    }
    if (!require(
            breathingWrite.error.contains(QStringLiteral("channel-wide")),
            "fixed-header native effect rejection should explain the shared-channel boundary"
        )) {
        return 1;
    }

    const AsusAuraHidProtocolResult colorCycleWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        1,
        lumacore::RgbEffect(lumacore::RgbEffectType::ColorCycle, lumacore::RgbColor(20, 40, 80), 1.0, 100),
        1
    );
    if (!require(colorCycleWrite.ok, "color cycle native effect should build")) {
        return 1;
    }
    if (!require(colorCycleWrite.packet.reports.size() == 2, "color cycle should contain gen1 and mode reports")) {
        return 1;
    }
    const QByteArray colorCycleModeReport = colorCycleWrite.packet.reports.at(1);
    if (!require(static_cast<unsigned char>(colorCycleModeReport.at(2)) == 0x01, "first addressable header should use effect channel 1")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(colorCycleModeReport.at(5)) == 0x04, "color cycle mode report should request mode 0x04")) {
        return 1;
    }
    const AsusAuraHidProtocolResult unsupportedBrightnessWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        1,
        lumacore::RgbEffect(lumacore::RgbEffectType::ColorCycle, lumacore::RgbColor(20, 40, 80), 1.0, 75),
        1
    );
    if (!require(
            !unsupportedBrightnessWrite.ok,
            "native effects should reject brightness values without a verified hardware representation"
        )) {
        return 1;
    }

    const AsusAuraHidProtocolResult rainbowWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        2,
        lumacore::RgbEffect(lumacore::RgbEffectType::Rainbow, lumacore::RgbColor(20, 40, 80), 1.0, 100),
        1
    );
    if (!require(rainbowWrite.ok, "rainbow native effect should build")) {
        return 1;
    }
    const QByteArray rainbowModeReport = rainbowWrite.packet.reports.at(1);
    if (!require(static_cast<unsigned char>(rainbowModeReport.at(2)) == 0x02, "second addressable header should use effect channel 2")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(rainbowModeReport.at(5)) == 0x05, "rainbow mode report should request mode 0x05")) {
        return 1;
    }
    const AsusAuraHidProtocolResult zeroBrightnessWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        2,
        lumacore::RgbEffect(lumacore::RgbEffectType::Rainbow, lumacore::RgbColor(20, 40, 80), 1.0, 0),
        1
    );
    if (!require(zeroBrightnessWrite.ok, "zero-brightness native effects should serialize as off")) {
        return 1;
    }
    if (!require(
            static_cast<unsigned char>(zeroBrightnessWrite.packet.reports.at(1).at(5)) == 0x00,
            "zero-brightness native effects should request off mode"
        )) {
        return 1;
    }

    const AsusAuraHidProtocolResult addressableBreathingWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        1,
        lumacore::RgbEffect(lumacore::RgbEffectType::Breathing, lumacore::RgbColor(20, 40, 80), 1.0, 75),
        1
    );
    if (!require(!addressableBreathingWrite.ok, "color-bearing native effects should not target addressable headers until EC36 mapping is verified")) {
        return 1;
    }
    if (!require(
            addressableBreathingWrite.error.contains(QStringLiteral("addressable headers")),
            "addressable native effect rejection should explain the safety boundary"
        )) {
        return 1;
    }

    const AsusAuraHidProtocolResult approvedOff = buildAsusAuraAllOffWrite(parsedConfig);
    if (!require(approvedOff.ok, "approved all-off write should build")) {
        return 1;
    }
    if (!require(approvedOff.packet.hardwareWriteApproved, "approved all-off should be hardware-approved")) {
        return 1;
    }
    if (!require(approvedOff.packet.kind == AsusAuraHidPacketKind::AllOffWrite, "all-off should be marked separately")) {
        return 1;
    }
    if (!require(approvedOff.packet.reports.size() == 4, "all-off should contain gen1 plus one off-mode report per config channel")) {
        return 1;
    }
    const QByteArray allOffModeReport = approvedOff.packet.reports.at(1);
    if (!require(static_cast<unsigned char>(allOffModeReport.at(0)) == 0xEC, "all-off report should use Aura command prefix")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(allOffModeReport.at(1)) == 0x35, "all-off report should use set-mode command")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(allOffModeReport.at(5)) == 0x00, "all-off report should request off mode")) {
        return 1;
    }

    if (!require(!buildAsusAuraStaticColorPreview(-1, lumacore::RgbColor(1, 2, 3), 1, 25).ok, "negative zones should be rejected")) {
        return 1;
    }
    if (!require(!buildAsusAuraStaticColorPreview(kAsusAuraHeaderCount, lumacore::RgbColor(1, 2, 3), 1, 25).ok, "zones beyond the header model should be rejected")) {
        return 1;
    }
    if (!require(!buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(1, 2, 3), 0, 25).ok, "zero LED count should be rejected")) {
        return 1;
    }
    if (!require(!buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(1, 2, 3), kAsusAuraMaxResearchLeds + 1, 25).ok, "oversized LED count should be rejected")) {
        return 1;
    }
    if (!require(!buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(1, 2, 3), 1, 101).ok, "brightness over 100 should be rejected")) {
        return 1;
    }
    if (!require(
            !buildAsusAuraStaticColorWrite(
                parsedConfig,
                kAsusAuraHeaderCount,
                lumacore::RgbColor(1, 2, 3),
                1,
                25
            ).ok,
            "approved writes should reject zones beyond the parsed config"
        )) {
        return 1;
    }
    if (!require(!buildAsusAuraNativeEffectWrite(
              parsedConfig,
              3,
              lumacore::RgbEffect(lumacore::RgbEffectType::Rainbow, lumacore::RgbColor(1, 2, 3), 1.0, 100),
              1
          ).ok,
          "native effect writes should reject zones beyond the parsed config")) {
        return 1;
    }

    // --- Golden test: EC40 direct-color encoding verified against the real hardware ---
    // Captured from a physical ASUS board (VID_0B05 / PID_19AF, AURA LED Controller) with the
    // board set to PURE RED, via USBPcap (LumaScope). Every direct-color packet on the wire was:
    //   EC 40 <channel | 0x80-on-last-chunk> <ledOffset> 0x14 <ff0000 x 20 LEDs>
    // i.e. report 0xEC / command 0x40, offset+count in LEDs (20 LEDs = 60 payload bytes), RGB
    // order (red = ff 00 00), apply (0x80) flag on the final chunk. Zone 1 is an addressable
    // header, whose static-color path uses this direct-color encoder.
    const AsusAuraHidProtocolResult capturedRed = buildAsusAuraStaticColorWrite(
        parsedConfig,
        1,
        lumacore::RgbColor(255, 0, 0),
        kAsusAuraMaxResearchLeds,
        100
    );
    if (!require(capturedRed.ok, "static red on an addressable header should build a direct-color write")) {
        return 1;
    }
    QVector<QByteArray> directChunks;
    for (const QByteArray& report : capturedRed.packet.reports) {
        if (report.size() >= 8
            && static_cast<unsigned char>(report.at(0)) == 0xEC
            && static_cast<unsigned char>(report.at(1)) == 0x40) {
            directChunks.append(report);
        }
    }
    if (!require(directChunks.size() == 6, "120 LEDs at 20 LEDs/packet should produce 6 EC40 direct-color chunks")) {
        return 1;
    }
    const QByteArray& firstChunk = directChunks.first();
    if (!require(
            static_cast<unsigned char>(firstChunk.at(3)) == 0x00
                && static_cast<unsigned char>(firstChunk.at(4)) == 20,
            "first direct chunk should start at LED 0 and cover 20 LEDs (offset/count are in LEDs)"
        )) {
        return 1;
    }
    // The crux verified on real hardware: ASUS Aura direct color is RGB (red -> ff 00 00) at
    // payload offset 5. A controlled single-color USBPcap capture confirmed this; if it ever
    // flips to GRB the board will display wrong colors.
    if (!require(
            static_cast<unsigned char>(firstChunk.at(5)) == 0xFF
                && static_cast<unsigned char>(firstChunk.at(6)) == 0x00
                && static_cast<unsigned char>(firstChunk.at(7)) == 0x00,
            "captured ground truth: direct color is RGB (red = ff 00 00) starting at payload byte 5"
        )) {
        return 1;
    }
    if (!require(
            (static_cast<unsigned char>(directChunks.last().at(2)) & 0x80) != 0
                && (static_cast<unsigned char>(directChunks.first().at(2)) & 0x80) == 0,
            "only the final direct chunk should set the apply (0x80) flag"
        )) {
        return 1;
    }
    for (int i = 0; i < directChunks.size(); ++i) {
        if (!require(
                static_cast<unsigned char>(directChunks.at(i).at(3)) == static_cast<unsigned char>(i * 20),
                "direct chunk LED offsets should step by 20 (0,20,40,60,80,100)"
            )) {
            return 1;
        }
    }

    return 0;
}
