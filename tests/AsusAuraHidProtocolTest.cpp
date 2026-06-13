#include "hardware/linux/AsusAuraHidProtocol.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QString>

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
    if (!require(inconsistentConfig.valid, "config responses with inconsistent RGB header counts should still parse")) {
        return 1;
    }
    if (!require(inconsistentConfig.rgbHeaderCount == 0, "RGB header counts beyond mainboard LEDs should be suppressed")) {
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

    const AsusAuraHidProtocolResult breathingWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        0,
        lumacore::RgbEffect(lumacore::RgbEffectType::Breathing, lumacore::RgbColor(20, 40, 80), 1.5, 25),
        1
    );
    if (!require(breathingWrite.ok, "breathing native effect should build")) {
        return 1;
    }
    if (!require(breathingWrite.packet.hardwareWriteApproved, "breathing native effect should be hardware-approved")) {
        return 1;
    }
    if (!require(breathingWrite.packet.kind == AsusAuraHidPacketKind::NativeEffectWrite, "breathing should be marked as a native effect write")) {
        return 1;
    }
    if (!require(breathingWrite.packet.reports.size() == 3, "breathing should contain gen1, mode, and color reports")) {
        return 1;
    }
    const QByteArray breathingModeReport = breathingWrite.packet.reports.at(1);
    const QByteArray breathingColorReport = breathingWrite.packet.reports.at(2);
    if (!require(static_cast<unsigned char>(breathingModeReport.at(1)) == 0x35, "breathing mode report should use EC35")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(breathingModeReport.at(5)) == 0x02, "breathing mode report should request mode 0x02")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(breathingColorReport.at(1)) == 0x36, "breathing color report should use EC36")) {
        return 1;
    }
    if (!require(static_cast<unsigned char>(breathingColorReport.at(11)) == 5, "breathing red channel should be brightness-scaled at fixed header offset")) {
        return 1;
    }

    const AsusAuraHidProtocolResult colorCycleWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        1,
        lumacore::RgbEffect(lumacore::RgbEffectType::ColorCycle, lumacore::RgbColor(20, 40, 80), 1.0, 75),
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
    if (!require(!buildAsusAuraStaticColorWrite(kAsusAuraHeaderCount, lumacore::RgbColor(1, 2, 3), 1, 25).ok, "approved writes should reject zones beyond the header model")) {
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

    return 0;
}