// SPDX-License-Identifier: GPL-2.0-or-later

// Unit coverage for the shared ASUS Aura HID protocol serializers: device
// identity and allowlist cataloging, EC B0/EC 30 config-table parsing and
// normalization, research previews, and the approved static-color, native
// effect, all-off, and direct-frame write builders. The expected packet bytes
// are validated against real USB captures (LumaScope, see
// docs/hardware/asus-aura-hid.md) and change only with new capture evidence.

#include "hardware/asus/AsusAuraHidProtocol.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QVector>

namespace {

using namespace lumacore::hardware::asus;

int g_failures = 0;

// Records a failure and keeps running so one run reports every broken
// expectation. Returns the condition so callers can early-return before
// dereferencing packet contents that a failed precondition leaves undefined.
bool check(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << "FAIL:" << message;
        ++g_failures;
    }
    return condition;
}

// Builds a synthetic EC30 config-table response with the given category counts
// at their verified table offsets (base 0x04: +0x02 addressable headers,
// +0x1B mainboard LEDs, +0x1D fixed RGB headers).
QByteArray makeEc30ConfigResponse(quint8 addressableHeaders, quint8 mainboardLeds, quint8 rgbHeaders)
{
    QByteArray response(kAsusAuraResearchReportLength, '\0');
    response[0] = static_cast<char>(0xEC);
    response[1] = static_cast<char>(0x30);
    response[0x04 + 0x02] = static_cast<char>(addressableHeaders);
    response[0x04 + 0x1B] = static_cast<char>(mainboardLeds);
    response[0x04 + 0x1D] = static_cast<char>(rgbHeaders);
    return response;
}

// The reference config shared by the write-builder tests: two addressable
// headers, three mainboard LEDs, and one fixed RGB header.
// testConfigTableParsing() asserts every property of this parse.
AsusAuraConfigTable makeVerifiedConfig()
{
    return parseAsusAuraConfigTableResponse(makeEc30ConfigResponse(2, 3, 1));
}

void testDeviceIdentityCatalog()
{
    check(asusAuraDeviceKey() == QStringLiteral("0B05:19AF"), "default ASUS Aura key should stay on the validated write target");
    check(isAsusAuraUsbVendor(QStringLiteral("0b05")), "ASUS Aura vendor matching should be case-insensitive");
    check(isAsusAuraWriteValidatedProduct(QStringLiteral("19af")), "19AF should be the only validated write product");
    check(!isAsusAuraWriteValidatedProduct(QStringLiteral("18F3")), "researched adjacent PIDs must not be write-validated");
    check(asusAuraResearchedDeviceKeys().contains(QStringLiteral("0B05:18F3")), "researched ASUS Aura keys should include adjacent documented PIDs");

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
        check(
            isAsusAuraResearchedUsbProduct(productId),
            "cataloged ASUS Aura PIDs should be treated as researched"
        );
        check(
            researchedDeviceKeys.contains(QStringLiteral("0B05:%1").arg(productId)),
            "researched ASUS Aura device key list should include every cataloged PID"
        );
        check(
            isAsusAuraWriteValidatedProduct(productId) == (productId == QStringLiteral("19AF")),
            "only 0B05:19AF should be write-validated"
        );
    }
}

void testConfigTableRequest()
{
    const QByteArray configRequest = buildAsusAuraConfigTableRequest();
    if (!check(configRequest.size() == kAsusAuraResearchReportLength, "config request should be 65 bytes")) {
        return;
    }
    check(static_cast<unsigned char>(configRequest.at(0)) == 0xEC, "config request should use Aura command prefix");
    check(static_cast<unsigned char>(configRequest.at(1)) == 0xB0, "config request should use ECB0 command");
}

void testConfigTableParsing()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();
    if (!check(parsedConfig.valid, "EC30 config response should parse")) {
        return;
    }
    check(isAsusAuraConfigTableWriteReady(parsedConfig), "parsed config should be write-ready");
    check(parsedConfig.addressableHeaderCount == 2, "config parser should extract addressable header count");
    check(parsedConfig.mainboardLedCount == 3, "config parser should extract mainboard LED count");
    check(parsedConfig.rgbHeaderCount == 1, "config parser should extract RGB header count");
    if (check(parsedConfig.channels.size() == 3, "config parser should build fixed plus addressable channels")) {
        check(
            parsedConfig.channels.at(1).ledCount == kAsusAuraMaxResearchLeds
                && parsedConfig.channels.at(2).ledCount == kAsusAuraMaxResearchLeds,
            "config parser should default addressable headers to the LumaScope-validated 120 LED EC40 capacity"
        );
    }

    QByteArray reportIdConfigResponse(kAsusAuraResearchReportLength + 1, '\0');
    reportIdConfigResponse[1] = static_cast<char>(0xEC);
    reportIdConfigResponse[2] = static_cast<char>(0x30);
    reportIdConfigResponse[1 + 0x04 + 0x02] = static_cast<char>(1);
    reportIdConfigResponse[1 + 0x04 + 0x1B] = static_cast<char>(4);
    reportIdConfigResponse[1 + 0x04 + 0x1D] = static_cast<char>(1);
    const AsusAuraConfigTable reportIdParsedConfig = parseAsusAuraConfigTableResponse(reportIdConfigResponse);
    check(reportIdParsedConfig.valid, "EC30 config responses with a leading report ID should parse");
    check(reportIdParsedConfig.addressableHeaderCount == 1, "report-ID config parser should extract addressable headers");
}

void testConfigTableRejection()
{
    check(!parseAsusAuraConfigTableResponse(QByteArray(8, '\0')).valid, "short config responses should be rejected");

    QByteArray invalidConfigResponse(kAsusAuraResearchReportLength, '\0');
    invalidConfigResponse[0] = static_cast<char>(0xEC);
    invalidConfigResponse[1] = static_cast<char>(0x31);
    check(!parseAsusAuraConfigTableResponse(invalidConfigResponse).valid, "non-EC30 config responses should be rejected");

    check(!parseAsusAuraConfigTableResponse(makeEc30ConfigResponse(0, 0, 0)).valid, "empty EC30 config tables should be rejected");
    check(
        !parseAsusAuraConfigTableResponse(makeEc30ConfigResponse(17, 0, 0)).valid,
        "addressable channel counts beyond the EC40 field should be rejected"
    );

    AsusAuraConfigTable emptyMarkedValidConfig;
    emptyMarkedValidConfig.valid = true;
    check(
        !isAsusAuraConfigTableWriteReady(emptyMarkedValidConfig),
        "valid flags without usable channels must not advertise write readiness"
    );
}

void testImpossibleHeaderNormalization()
{
    const AsusAuraConfigTable impossibleHeaderConfig = parseAsusAuraConfigTableResponse(makeEc30ConfigResponse(0, 2, 4));
    check(impossibleHeaderConfig.valid, "impossible fixed RGB header counts should not reject the whole config table");
    check(
        impossibleHeaderConfig.rgbHeaderCount == 0,
        "impossible fixed RGB header counts should be normalized to no exposed fixed headers"
    );
    check(
        impossibleHeaderConfig.summary.contains(QStringLiteral("reportedRgbHeaders=4")),
        "normalized configs should preserve the raw fixed RGB header count in the summary"
    );

    const AsusAuraHidProtocolResult normalizedStaticWrite = buildAsusAuraStaticColorWrite(
        impossibleHeaderConfig,
        0,
        lumacore::RgbColor(12, 24, 36),
        2,
        100
    );
    if (!check(normalizedStaticWrite.ok, "normalized fixed-mainboard config should still allow aggregate static writes")) {
        return;
    }
    const QByteArray normalizedColorReport = normalizedStaticWrite.packet.reports.at(2);
    check(
        static_cast<unsigned char>(normalizedColorReport.at(2)) == 0x00
            && static_cast<unsigned char>(normalizedColorReport.at(3)) == 0x03,
        "normalized fixed-mainboard writes should target the verified mainboard LED mask"
    );
    check(
        normalizedStaticWrite.packet.summary.contains(QStringLiteral("fixedMainboard=true")),
        "normalized fixed-mainboard writes should identify the aggregate target"
    );
}

void testWideMainboardConfigs()
{
    const AsusAuraConfigTable wideMainboardOnlyConfig = parseAsusAuraConfigTableResponse(makeEc30ConfigResponse(0, 20, 0));
    check(
        !wideMainboardOnlyConfig.valid,
        "mainboard channels beyond the effect-color mask with no addressable headers should be rejected"
    );
    check(
        wideMainboardOnlyConfig.error.contains(QStringLiteral("effect-color mask")),
        "rejected wide mainboard configs should explain the mask limit"
    );

    const AsusAuraConfigTable wideMainboardMixedConfig = parseAsusAuraConfigTableResponse(makeEc30ConfigResponse(1, 20, 0));
    check(
        wideMainboardMixedConfig.valid,
        "wide mainboard channels should not reject configs that still expose addressable headers"
    );
    check(
        asusAuraFixedExposedZoneCount(wideMainboardMixedConfig) == 0,
        "mainboard channels beyond the effect-color mask should not be exposed as a zone"
    );
    check(
        asusAuraExposedZoneCount(wideMainboardMixedConfig) == 1,
        "wide mainboard configs should expose only their addressable headers"
    );
    check(
        wideMainboardMixedConfig.summary.contains(QStringLiteral("aggregate mainboard zone is not exposed")),
        "wide mainboard configs should note the unexposed aggregate zone in the summary"
    );

    const AsusAuraHidProtocolResult wideMainboardWrite = buildAsusAuraStaticColorWrite(
        wideMainboardMixedConfig,
        0,
        lumacore::RgbColor(12, 24, 36),
        2,
        100
    );
    check(
        wideMainboardWrite.ok,
        "zone 0 of a wide mainboard config should resolve to the first addressable header"
    );
    check(
        wideMainboardWrite.packet.summary.contains(QStringLiteral("addressableHeader=1")),
        "wide mainboard configs should route zone 0 writes to the addressable channel"
    );
}

void testStaticColorPreviews()
{
    const AsusAuraHidProtocolResult valid = buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(20, 40, 80), 1, 25);
    if (!check(valid.ok, "valid static color packet should build")) {
        return;
    }
    check(valid.packet.report.size() == kAsusAuraResearchReportLength, "report length should match research length");
    check(!valid.packet.hardwareWriteApproved, "research packet must not be hardware-approved");
    check(valid.packet.kind == AsusAuraHidPacketKind::ResearchPreview, "preview packet should be marked as research");
    check(valid.packet.reports.size() == 1, "preview packet should expose one report");
    check(static_cast<unsigned char>(valid.packet.report.at(8)) == 5, "red channel should be brightness-scaled");
    check(static_cast<unsigned char>(valid.packet.report.at(9)) == 10, "green channel should be brightness-scaled");
    check(static_cast<unsigned char>(valid.packet.report.at(10)) == 20, "blue channel should be brightness-scaled");
    check(static_cast<unsigned char>(valid.packet.report.at(11)) == 0, "header 1 should encode zone index 0");
    check(valid.packet.summary.contains(QStringLiteral("Header 1")), "summary should include the header label");

    const AsusAuraHidProtocolResult header3 = buildAsusAuraStaticColorPreview(2, lumacore::RgbColor(1, 2, 3), 30, 100);
    if (check(header3.ok, "header 3 preview should build")) {
        check(static_cast<unsigned char>(header3.packet.report.at(11)) == 2, "header 3 should encode zone index 2");
        check(header3.packet.summary.contains(QStringLiteral("Header 3")), "header 3 summary should include the header label");
    }

    const AsusAuraHidProtocolResult allOff = buildAsusAuraStaticColorPreview(1, lumacore::RgbColor(0, 0, 0), 30, 100);
    if (check(allOff.ok, "all-off static preview should build")) {
        check(allOff.packet.summary.contains(QStringLiteral("allOffPreview=true")), "all-off preview should be marked in the summary");
    }
}

void testFixedHeaderStaticColorWrite()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();
    const AsusAuraHidProtocolResult staticWrite = buildAsusAuraStaticColorWrite(parsedConfig, 0, lumacore::RgbColor(20, 40, 80), 10, 25);
    if (!check(staticWrite.ok, "approved static color write should build")) {
        return;
    }
    check(staticWrite.packet.hardwareWriteApproved, "approved static color write should be hardware-approved");
    check(staticWrite.packet.kind == AsusAuraHidPacketKind::StaticColorWrite, "static write should be marked as a hardware write");
    if (!check(staticWrite.packet.reports.size() == 3, "fixed static write should contain gen1, mode, and color reports")) {
        return;
    }
    const QByteArray gen1Report = staticWrite.packet.reports.at(0);
    const QByteArray modeReport = staticWrite.packet.reports.at(1);
    const QByteArray colorReport = staticWrite.packet.reports.at(2);
    check(static_cast<unsigned char>(gen1Report.at(0)) == 0xEC, "gen1 report should use Aura command prefix");
    check(static_cast<unsigned char>(gen1Report.at(1)) == 0x52, "gen1 report should use EC52 command");
    check(modeReport.size() == kAsusAuraResearchReportLength, "mode report should be 65 bytes");
    check(colorReport.size() == kAsusAuraResearchReportLength, "color report should be 65 bytes");
    check(static_cast<unsigned char>(modeReport.at(0)) == 0xEC, "mode report should use Aura command prefix");
    check(static_cast<unsigned char>(modeReport.at(1)) == 0x35, "mode report should use EC35 set-mode command");
    check(static_cast<unsigned char>(modeReport.at(5)) == 0x01, "mode report should request static mode");
    check(static_cast<unsigned char>(colorReport.at(0)) == 0xEC, "color report should use Aura command prefix");
    check(static_cast<unsigned char>(colorReport.at(1)) == 0x36, "color report should use EC36 set-colors command");
    check(static_cast<unsigned char>(colorReport.at(2)) == 0x00, "color report high mask should match fixed RGB header target");
    check(static_cast<unsigned char>(colorReport.at(3)) == 0x04, "color report low mask should match fixed RGB header target");
    check(static_cast<unsigned char>(colorReport.at(11)) == 5, "approved red channel should be brightness-scaled at fixed header offset");
    check(static_cast<unsigned char>(colorReport.at(12)) == 10, "approved green channel should be brightness-scaled at fixed header offset");
    check(static_cast<unsigned char>(colorReport.at(13)) == 20, "approved blue channel should be brightness-scaled at fixed header offset");
    check(staticWrite.packet.provenance.contains(QStringLiteral("OpenRGB")), "approved write should carry provenance");
}

void testAddressableStaticColorWrite()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();
    const AsusAuraHidProtocolResult addressableWrite = buildAsusAuraStaticColorWrite(parsedConfig, 1, lumacore::RgbColor(20, 40, 80), 30, 25);
    if (!check(addressableWrite.ok, "addressable static write should build")) {
        return;
    }
    if (!check(addressableWrite.packet.reports.size() == 4, "addressable write should contain gen1, direct-mode, and chunked direct color reports")) {
        return;
    }
    const QByteArray directModeReport = addressableWrite.packet.reports.at(1);
    const QByteArray firstDirectReport = addressableWrite.packet.reports.at(2);
    const QByteArray finalDirectReport = addressableWrite.packet.reports.at(3);
    check(static_cast<unsigned char>(directModeReport.at(5)) == 0xFF, "addressable mode report should request direct mode");
    check(static_cast<unsigned char>(firstDirectReport.at(1)) == 0x40, "addressable color report should use EC40 direct command");
    check(static_cast<unsigned char>(firstDirectReport.at(2)) == 0x00, "first direct report should not set apply bit");
    check(static_cast<unsigned char>(finalDirectReport.at(2)) == 0x80, "final direct report should set apply bit");

    AsusAuraConfigTable invalidDirectChannelConfig = parsedConfig;
    invalidDirectChannelConfig.channels[1].directChannel = 16;
    check(
        !buildAsusAuraStaticColorWrite(
            invalidDirectChannelConfig,
            1,
            lumacore::RgbColor(20, 40, 80),
            30,
            25
        ).ok,
        "out-of-range direct channels should be rejected instead of aliased"
    );
}

void testNativeEffectBoundaries()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();

    const AsusAuraHidProtocolResult breathingWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        0,
        lumacore::RgbEffect(lumacore::RgbEffectType::Breathing, lumacore::RgbColor(20, 40, 80), 1.5, 25),
        1
    );
    check(!breathingWrite.ok, "fixed-header native effects should be rejected because their effect channel is shared");
    check(
        breathingWrite.error.contains(QStringLiteral("channel-wide")),
        "fixed-header native effect rejection should explain the shared-channel boundary"
    );

    const AsusAuraHidProtocolResult unsupportedBrightnessWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        1,
        lumacore::RgbEffect(lumacore::RgbEffectType::ColorCycle, lumacore::RgbColor(20, 40, 80), 1.0, 75),
        1
    );
    check(
        !unsupportedBrightnessWrite.ok,
        "native effects should reject brightness values without a verified hardware representation"
    );

    const AsusAuraHidProtocolResult addressableBreathingWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        1,
        lumacore::RgbEffect(lumacore::RgbEffectType::Breathing, lumacore::RgbColor(20, 40, 80), 1.0, 75),
        1
    );
    check(!addressableBreathingWrite.ok, "color-bearing native effects should not target addressable headers until EC36 mapping is verified");
    check(
        addressableBreathingWrite.error.contains(QStringLiteral("addressable headers")),
        "addressable native effect rejection should explain the safety boundary"
    );
}

void testAddressableNativeEffects()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();

    const AsusAuraHidProtocolResult colorCycleWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        1,
        lumacore::RgbEffect(lumacore::RgbEffectType::ColorCycle, lumacore::RgbColor(20, 40, 80), 1.0, 100),
        1
    );
    if (check(colorCycleWrite.ok, "color cycle native effect should build")
        && check(colorCycleWrite.packet.reports.size() == 2, "color cycle should contain gen1 and mode reports")) {
        const QByteArray colorCycleModeReport = colorCycleWrite.packet.reports.at(1);
        check(static_cast<unsigned char>(colorCycleModeReport.at(2)) == 0x01, "first addressable header should use effect channel 1");
        check(static_cast<unsigned char>(colorCycleModeReport.at(5)) == 0x04, "color cycle mode report should request mode 0x04");
    }

    const AsusAuraHidProtocolResult rainbowWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        2,
        lumacore::RgbEffect(lumacore::RgbEffectType::Rainbow, lumacore::RgbColor(20, 40, 80), 1.0, 100),
        1
    );
    if (check(rainbowWrite.ok, "rainbow native effect should build")) {
        const QByteArray rainbowModeReport = rainbowWrite.packet.reports.at(1);
        check(static_cast<unsigned char>(rainbowModeReport.at(2)) == 0x02, "second addressable header should use effect channel 2");
        check(static_cast<unsigned char>(rainbowModeReport.at(5)) == 0x05, "rainbow mode report should request mode 0x05");
    }

    const AsusAuraHidProtocolResult zeroBrightnessWrite = buildAsusAuraNativeEffectWrite(
        parsedConfig,
        2,
        lumacore::RgbEffect(lumacore::RgbEffectType::Rainbow, lumacore::RgbColor(20, 40, 80), 1.0, 0),
        1
    );
    if (check(zeroBrightnessWrite.ok, "zero-brightness native effects should serialize as off")) {
        check(
            static_cast<unsigned char>(zeroBrightnessWrite.packet.reports.at(1).at(5)) == 0x00,
            "zero-brightness native effects should request off mode"
        );
    }
}

void testAllOffWrite()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();
    const AsusAuraHidProtocolResult approvedOff = buildAsusAuraAllOffWrite(parsedConfig);
    if (!check(approvedOff.ok, "approved all-off write should build")) {
        return;
    }
    check(approvedOff.packet.hardwareWriteApproved, "approved all-off should be hardware-approved");
    check(approvedOff.packet.kind == AsusAuraHidPacketKind::AllOffWrite, "all-off should be marked separately");
    if (!check(approvedOff.packet.reports.size() == 4, "all-off should contain gen1 plus one off-mode report per config channel")) {
        return;
    }
    const QByteArray allOffModeReport = approvedOff.packet.reports.at(1);
    check(static_cast<unsigned char>(allOffModeReport.at(0)) == 0xEC, "all-off report should use Aura command prefix");
    check(static_cast<unsigned char>(allOffModeReport.at(1)) == 0x35, "all-off report should use set-mode command");
    check(static_cast<unsigned char>(allOffModeReport.at(5)) == 0x00, "all-off report should request off mode");
}

void testInputValidation()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();
    check(!buildAsusAuraStaticColorPreview(-1, lumacore::RgbColor(1, 2, 3), 1, 25).ok, "negative zones should be rejected");
    check(!buildAsusAuraStaticColorPreview(kAsusAuraHeaderCount, lumacore::RgbColor(1, 2, 3), 1, 25).ok, "zones beyond the header model should be rejected");
    check(!buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(1, 2, 3), 0, 25).ok, "zero LED count should be rejected");
    check(!buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(1, 2, 3), kAsusAuraMaxResearchLeds + 1, 25).ok, "oversized LED count should be rejected");
    check(!buildAsusAuraStaticColorPreview(0, lumacore::RgbColor(1, 2, 3), 1, 101).ok, "brightness over 100 should be rejected");
    check(
        !buildAsusAuraStaticColorWrite(
            parsedConfig,
            kAsusAuraHeaderCount,
            lumacore::RgbColor(1, 2, 3),
            1,
            25
        ).ok,
        "approved writes should reject zones beyond the parsed config"
    );
    check(
        !buildAsusAuraNativeEffectWrite(
            parsedConfig,
            3,
            lumacore::RgbEffect(lumacore::RgbEffectType::Rainbow, lumacore::RgbColor(1, 2, 3), 1.0, 100),
            1
        ).ok,
        "native effect writes should reject zones beyond the parsed config"
    );
}

void testDirectFrameStreaming()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();
    QVector<lumacore::RgbColor> frameColors;
    for (int index = 0; index < 21; ++index) {
        frameColors.append(lumacore::RgbColor(
            static_cast<quint8>(index),
            static_cast<quint8>(index + 1),
            static_cast<quint8>(index + 2)
        ));
    }

    const AsusAuraHidProtocolResult primedFrame =
        buildAsusAuraDirectFrameWrite(parsedConfig, 1, frameColors, true);
    if (!check(primedFrame.ok, "addressable direct frame write should build")) {
        return;
    }
    check(primedFrame.packet.kind == AsusAuraHidPacketKind::DirectFrameWrite, "direct frame write should use its own packet kind");
    check(primedFrame.packet.hardwareWriteApproved, "direct frame write should be hardware-approved");
    if (check(primedFrame.packet.reports.size() == 4, "primed 21-LED frame should contain gen1, direct mode, and two EC40 chunks")) {
        const QByteArray primedModeReport = primedFrame.packet.reports.at(1);
        const QByteArray frameFirstChunk = primedFrame.packet.reports.at(2);
        const QByteArray frameFinalChunk = primedFrame.packet.reports.at(3);
        check(static_cast<unsigned char>(primedModeReport.at(5)) == 0xFF, "primed frame should request direct mode");
        check(static_cast<unsigned char>(frameFirstChunk.at(1)) == 0x40, "frame chunks should use EC40 direct color");
        check(
            static_cast<unsigned char>(frameFirstChunk.at(5)) == 0
                && static_cast<unsigned char>(frameFirstChunk.at(6)) == 1
                && static_cast<unsigned char>(frameFirstChunk.at(7)) == 2,
            "frame chunks should preserve per-LED RGB payload order"
        );
        check(
            static_cast<unsigned char>(frameFirstChunk.at(3)) == 0
                && static_cast<unsigned char>(frameFirstChunk.at(4)) == 20,
            "first frame chunk should cover the first 20 LEDs"
        );
        check(
            static_cast<unsigned char>(frameFinalChunk.at(3)) == 20
                && static_cast<unsigned char>(frameFinalChunk.at(4)) == 1,
            "final frame chunk should carry the remaining LED offset/count in LEDs"
        );
        check(
            (static_cast<unsigned char>(frameFinalChunk.at(2)) & 0x80) != 0
                && (static_cast<unsigned char>(frameFirstChunk.at(2)) & 0x80) == 0,
            "direct frame writes should set the apply flag only on the final chunk"
        );
    }

    const AsusAuraHidProtocolResult frameOnly =
        buildAsusAuraDirectFrameWrite(parsedConfig, 1, frameColors, false);
    if (check(frameOnly.ok, "unprimed direct frame write should build")
        && check(frameOnly.packet.reports.size() == 2, "unprimed frame writes should contain only EC40 chunks")) {
        check(
            static_cast<unsigned char>(frameOnly.packet.reports.first().at(1)) == 0x40,
            "unprimed frame writes should start with EC40"
        );
    }

    check(
        !buildAsusAuraDirectFrameWrite(parsedConfig, 0, frameColors, true).ok,
        "direct frame streaming should reject fixed RGB headers"
    );
    check(
        !buildAsusAuraDirectFrameWrite(parsedConfig, 1, {}, true).ok,
        "direct frame streaming should reject empty frames"
    );
}

// --- Golden test: EC40 direct-color encoding verified against the real hardware ---
// Captured from a physical ASUS board (VID_0B05 / PID_19AF, AURA LED Controller) with the
// board set to PURE RED, via USBPcap (LumaScope). Every direct-color packet on the wire was:
//   EC 40 <channel | 0x80-on-last-chunk> <ledOffset> 0x14 <ff0000 x 20 LEDs>
// i.e. report 0xEC / command 0x40, offset+count in LEDs (20 LEDs = 60 payload bytes), RGB
// order (red = ff 00 00), apply (0x80) flag on the final chunk. Zone 1 is an addressable
// header, whose static-color path uses this direct-color encoder.
void testCapturedRedGoldenFrame()
{
    const AsusAuraConfigTable parsedConfig = makeVerifiedConfig();
    const AsusAuraHidProtocolResult capturedRed = buildAsusAuraStaticColorWrite(
        parsedConfig,
        1,
        lumacore::RgbColor(255, 0, 0),
        kAsusAuraMaxResearchLeds,
        100
    );
    if (!check(capturedRed.ok, "static red on an addressable header should build a direct-color write")) {
        return;
    }
    QVector<QByteArray> directChunks;
    for (const QByteArray& report : capturedRed.packet.reports) {
        if (report.size() >= 8
            && static_cast<unsigned char>(report.at(0)) == 0xEC
            && static_cast<unsigned char>(report.at(1)) == 0x40) {
            directChunks.append(report);
        }
    }
    if (!check(directChunks.size() == 6, "120 LEDs at 20 LEDs/packet should produce 6 EC40 direct-color chunks")) {
        return;
    }
    const QByteArray& firstChunk = directChunks.first();
    check(
        static_cast<unsigned char>(firstChunk.at(3)) == 0x00
            && static_cast<unsigned char>(firstChunk.at(4)) == 20,
        "first direct chunk should start at LED 0 and cover 20 LEDs (offset/count are in LEDs)"
    );
    // The crux verified on real hardware: ASUS Aura direct color is RGB (red -> ff 00 00) at
    // payload offset 5. A controlled single-color USBPcap capture confirmed this; if it ever
    // flips to GRB the board will display wrong colors.
    check(
        static_cast<unsigned char>(firstChunk.at(5)) == 0xFF
            && static_cast<unsigned char>(firstChunk.at(6)) == 0x00
            && static_cast<unsigned char>(firstChunk.at(7)) == 0x00,
        "captured ground truth: direct color is RGB (red = ff 00 00) starting at payload byte 5"
    );
    check(
        (static_cast<unsigned char>(directChunks.last().at(2)) & 0x80) != 0
            && (static_cast<unsigned char>(directChunks.first().at(2)) & 0x80) == 0,
        "only the final direct chunk should set the apply (0x80) flag"
    );
    for (int i = 0; i < directChunks.size(); ++i) {
        check(
            static_cast<unsigned char>(directChunks.at(i).at(3)) == static_cast<unsigned char>(i * 20),
            "direct chunk LED offsets should step by 20 (0,20,40,60,80,100)"
        );
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    testDeviceIdentityCatalog();
    testConfigTableRequest();
    testConfigTableParsing();
    testConfigTableRejection();
    testImpossibleHeaderNormalization();
    testWideMainboardConfigs();
    testStaticColorPreviews();
    testFixedHeaderStaticColorWrite();
    testAddressableStaticColorWrite();
    testNativeEffectBoundaries();
    testAddressableNativeEffects();
    testAllOffWrite();
    testInputValidation();
    testDirectFrameStreaming();
    testCapturedRedGoldenFrame();

    if (g_failures != 0) {
        qCritical().noquote() << g_failures << "ASUS Aura HID protocol check(s) failed";
        return 1;
    }
    return 0;
}
