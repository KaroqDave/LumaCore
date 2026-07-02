// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/asus/AsusAuraHidBackend.h"

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

    return 0;
}
