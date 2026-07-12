// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/asus/AsusAuraHidBackend.h"
#include "backends/asus/AsusAuraHidDevice.h"
#include "hardware/asus/AsusAuraHidProtocol.h"

#include <QCoreApplication>
#include <QDebug>

#include <utility>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

lumacore::asus_aura_platform::ProbeDevice auraProbeDevice(
    QString productId = QStringLiteral("19AF"),
    QString path = QStringLiteral("test-hid-path"),
    int interfaceNumber = -1
)
{
    return {
        QStringLiteral("hidapi"),
        QStringLiteral("asus-aura-test"),
        QStringLiteral("AURA LED Controller"),
        QStringLiteral("ASUSTek Computer, Inc."),
        std::move(path),
        QStringLiteral("VID:PID 0B05:%1").arg(productId),
        QStringLiteral("0B05"),
        std::move(productId),
        interfaceNumber,
        0xFF00,
        0,
        lumacore::RgbDeviceType::Controller,
    };
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    const lumacore::asus_aura_platform::ProbeDevice unknownInterface =
        auraProbeDevice(QStringLiteral("19AF"), QStringLiteral("test-hid-path"), -1);
    const lumacore::asus_aura_platform::ProbeDevice numberedInterface =
        auraProbeDevice(QStringLiteral("19AF"), QStringLiteral("test-hid-path-2"), 2);
    const lumacore::asus_aura_platform::ProbeDevice emptyPath =
        auraProbeDevice(QStringLiteral("19AF"), QString(), -1);
    const lumacore::asus_aura_platform::ProbeDevice researchOnly =
        auraProbeDevice(QStringLiteral("18F3"), QStringLiteral("test-hid-path"), -1);

    if (!require(
            lumacore::AsusAuraHidBackend::acceptsProbeDevice(unknownInterface),
            "validated ASUS Aura HID devices with a path should remain candidates even when the interface number is unknown"
        )
        || !require(
            !lumacore::AsusAuraHidBackend::acceptsProbeDevice(emptyPath),
            "ASUS Aura HID candidates still require a writable HID path"
        )
        || !require(
            !lumacore::AsusAuraHidBackend::acceptsProbeDevice(researchOnly),
            "research-only ASUS Aura identities must not enter the guarded write backend"
        )
        || !require(
            lumacore::AsusAuraHidBackend::prefersProbeDevice(numberedInterface, unknownInterface),
            "known interface numbers should still win over unknown interface numbers"
        )
        || !require(
            !lumacore::AsusAuraHidBackend::prefersProbeDevice(unknownInterface, numberedInterface),
            "unknown interface numbers should not displace known interface numbers"
        )) {
        return 1;
    }

    lumacore::hardware::asus::AsusAuraConfigTable config;
    config.valid = true;
    config.addressableHeaderCount = 1;
    config.mainboardLedCount = 3;
    config.rgbHeaderCount = 1;
    config.channels = {
        {
            lumacore::hardware::asus::AsusAuraChannelType::Fixed,
            0,
            0,
            3,
            1,
        },
        {
            lumacore::hardware::asus::AsusAuraChannelType::Addressable,
            1,
            1,
            20,
            0,
        },
    };
    lumacore::AsusAuraHidDevice device(
        auraProbeDevice(QStringLiteral("19AF"), QStringLiteral("test-hid-path"), 2),
        true,
        config,
        QStringLiteral("verified test config")
    );
    if (!require(device.zones().size() == 2, "config-derived ASUS fixture should expose fixed and addressable zones")) {
        return 1;
    }
    if (!require(
            !device.updateZoneMetadata(0, QStringLiteral("Header 1"), 2),
            "fixed ASUS zones should reject LED-count edits that drift from protocol geometry"
        )
        || !require(
            device.zones().at(0).ledCount() == 1,
            "rejected fixed-zone LED-count edits should leave metadata unchanged"
        )
        || !require(
            device.updateZoneMetadata(0, QStringLiteral("Renamed Header"), 1),
            "fixed ASUS zones should still allow metadata renames when the config LED count is preserved"
        )
        || !require(
            device.updateZoneMetadata(1, QStringLiteral("Addressable Header"), 30),
            "addressable ASUS zones should keep their editable LED count"
        )
        || !require(
            device.zones().at(1).ledCount() == 30,
            "addressable ASUS LED-count edits should be retained"
        )) {
        return 1;
    }

    if (!require(
            device.supportsZoneEffect(1, static_cast<int>(lumacore::RgbEffectType::Wave)),
            "addressable ASUS zones should support the host-streamed Wave effect"
        )
        || !require(
            device.supportsZoneEffect(1, static_cast<int>(lumacore::RgbEffectType::Marquee)),
            "addressable ASUS zones should support the host-streamed Marquee effect"
        )
        || !require(
            device.supportsZoneEffect(1, static_cast<int>(lumacore::RgbEffectType::Strobe)),
            "addressable ASUS zones should support the host-streamed Strobe effect"
        )
        || !require(
            !device.supportsZoneEffect(0, static_cast<int>(lumacore::RgbEffectType::Wave)),
            "fixed ASUS zones must not offer host-streamed animated effects"
        )
        || !require(
            !device.supportsZoneEffect(0, static_cast<int>(lumacore::RgbEffectType::Strobe)),
            "fixed ASUS zones must not offer the host-streamed Strobe effect"
        )) {
        return 1;
    }

    return 0;
}
