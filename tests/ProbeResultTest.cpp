// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/windows/WindowsDiscoveryBackend.h"
#include "hardware/linux/ProbeResult.h"
#include "hardware/windows/HidDeviceWriter.h"
#include "hardware/windows/ProbeResult.h"

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

lumacore::hardware::linux::ProbeDevice probeDevice(
    QString vendorId,
    QString productId,
    QString vendor,
    QString name,
    QString details = {}
)
{
    return {
        QStringLiteral("test"),
        lumacore::hardware::linux::stableProbeId(
            QStringLiteral("test"),
            QStringLiteral("%1-%2").arg(vendorId, productId)
        ),
        std::move(name),
        std::move(vendor),
        QStringLiteral("test-path"),
        std::move(details),
        std::move(vendorId),
        std::move(productId),
        -1,
        0,
        0,
        lumacore::RgbDeviceType::Controller,
    };
}

lumacore::hardware::windows::ProbeDevice windowsProbeDevice(
    QString vendorId,
    QString productId,
    QString vendor,
    QString name,
    QString details = {},
    QString path = QStringLiteral("test-path")
)
{
    return {
        QStringLiteral("test"),
        lumacore::hardware::windows::stableProbeId(
            QStringLiteral("test"),
            QStringLiteral("%1-%2-%3").arg(vendorId, productId, path)
        ),
        std::move(name),
        std::move(vendor),
        std::move(path),
        std::move(details),
        std::move(vendorId),
        std::move(productId),
        -1,
        0,
        0,
        lumacore::RgbDeviceType::Controller,
    };
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    using namespace lumacore::hardware::linux;

    const ProbeDevice validatedAsus = probeDevice(
        QStringLiteral("0b05"),
        QStringLiteral("19af"),
        QStringLiteral("ASUSTek Computer, Inc."),
        QStringLiteral("AURA LED Controller")
    );
    const DiscoverySupportInfo validatedSupport = discoverySupportInfo(validatedAsus);

    const ProbeDevice researchedAsus = probeDevice(
        QStringLiteral("0B05"),
        QStringLiteral("18F3"),
        QStringLiteral("ASUSTek Computer, Inc."),
        QStringLiteral("AURA LED Controller")
    );
    const DiscoverySupportInfo researchedSupport = discoverySupportInfo(researchedAsus);

    const ProbeDevice researchedAuraAddressable = probeDevice(
        QStringLiteral("0B05"),
        QStringLiteral("1867"),
        QStringLiteral("ASUSTek Computer, Inc."),
        QStringLiteral("AURA LED Controller")
    );
    const DiscoverySupportInfo researchedAddressableSupport =
        discoverySupportInfo(researchedAuraAddressable);

    const ProbeDevice researchedAuraTerminal = probeDevice(
        QStringLiteral("0B05"),
        QStringLiteral("1889"),
        QStringLiteral("ASUSTek Computer, Inc."),
        QStringLiteral("ROG AURA Terminal")
    );
    const DiscoverySupportInfo researchedTerminalSupport =
        discoverySupportInfo(researchedAuraTerminal);

    const ProbeDevice heuristicController = probeDevice(
        QStringLiteral("1234"),
        QStringLiteral("5678"),
        QStringLiteral("Example"),
        QStringLiteral("Example RGB Controller")
    );
    const DiscoverySupportInfo heuristicSupport = discoverySupportInfo(heuristicController);

    const ProbeDevice unrelated = probeDevice(
        QStringLiteral("1234"),
        QStringLiteral("9999"),
        QStringLiteral("Example"),
        QStringLiteral("Keyboard")
    );
    const DiscoverySupportInfo unrelatedSupport = discoverySupportInfo(unrelated);

    const lumacore::hardware::windows::ProbeDevice windowsValidatedAsus = windowsProbeDevice(
        QStringLiteral("0b05"),
        QStringLiteral("19af"),
        QStringLiteral("ASUSTek Computer, Inc."),
        QStringLiteral("AURA LED Controller")
    );
    const lumacore::hardware::windows::DiscoverySupportInfo windowsValidatedSupport =
        lumacore::hardware::windows::discoverySupportInfo(windowsValidatedAsus);
    const lumacore::hardware::windows::ProbeDevice windowsHeuristicController = windowsProbeDevice(
        QStringLiteral("1234"),
        QStringLiteral("5678"),
        QStringLiteral("Example"),
        QStringLiteral("Example Lighting Controller")
    );
    const lumacore::hardware::windows::DiscoverySupportInfo windowsHeuristicSupport =
        lumacore::hardware::windows::discoverySupportInfo(windowsHeuristicController);
    const lumacore::hardware::windows::HidDeviceWriter windowsWriter;
    const lumacore::hardware::windows::HidWriteResult emptyWindowsWrite =
        windowsWriter.writeReports(QString(), {});
    const lumacore::hardware::windows::ProbeDevice firstIdenticalController = windowsProbeDevice(
        QStringLiteral("1234"),
        QStringLiteral("abcd"),
        QStringLiteral("Example"),
        QStringLiteral("Example RGB Controller"),
        {},
        QStringLiteral("\\\\?\\hid#first")
    );
    const lumacore::hardware::windows::ProbeDevice secondIdenticalController = windowsProbeDevice(
        QStringLiteral("1234"),
        QStringLiteral("abcd"),
        QStringLiteral("Example"),
        QStringLiteral("Example RGB Controller"),
        {},
        QStringLiteral("\\\\?\\hid#second")
    );
    const lumacore::hardware::windows::ProbeDevice duplicateCollection = firstIdenticalController;
    const QVector<lumacore::hardware::windows::ProbeDevice> inventoryDevices =
        lumacore::windowsDiscoveryInventoryDevices({
            {
                QStringLiteral("test-provider"),
                true,
                {
                    firstIdenticalController,
                    secondIdenticalController,
                    duplicateCollection,
                },
                {},
            },
        });

    if (!require(
            stableProbeId(QStringLiteral("hid"), QStringLiteral("0B05:19AF /dev/hidraw0"))
                == QStringLiteral("hid-0b05-19af-dev-hidraw0"),
            "stable probe IDs should be lowercase and path-safe"
        )
        || !require(
            usbVidPidKey(validatedAsus) == QStringLiteral("0B05:19AF"),
            "VID:PID keys should be normalized to uppercase"
        )
        || !require(validatedSupport.cataloged, "validated ASUS identity should be cataloged")
        || !require(
            validatedSupport.stage == QStringLiteral("guarded-write-backend"),
            "validated ASUS identity should point at the guarded write backend stage"
        )
        || !require(
            validatedSupport.writeCapableBackend,
            "validated ASUS identity should record that a separate guarded backend exists"
        )
        || !require(researchedSupport.cataloged, "research ASUS identity should be cataloged")
        || !require(
            researchedSupport.stage == QStringLiteral("research-only"),
            "research ASUS identity should remain research-only"
        )
        || !require(
            !researchedSupport.writeCapableBackend,
            "research-only identities must not advertise write-capable backends"
        )
        || !require(
            researchedAddressableSupport.cataloged
                && researchedAddressableSupport.stage == QStringLiteral("research-only")
                && researchedAddressableSupport.family == QStringLiteral("ASUS Aura USB HID")
                && !researchedAddressableSupport.writeCapableBackend,
            "newly cataloged ASUS Aura addressable controllers should be research-only, in-family, and never write-capable"
        )
        || !require(
            researchedTerminalSupport.cataloged
                && researchedTerminalSupport.stage == QStringLiteral("research-only")
                && !researchedTerminalSupport.writeCapableBackend,
            "the ASUS ROG Aura Terminal identity should be cataloged research-only without a write backend"
        )
        || !require(
            heuristicSupport.stage == QStringLiteral("heuristic"),
            "keyword matches should be marked as heuristic only"
        )
        || !require(
            !heuristicSupport.cataloged && heuristicSupport.likelyRgbController,
            "heuristic matches should be likely RGB without becoming cataloged"
        )
        || !require(
            unrelatedSupport.stage == QStringLiteral("unclassified"),
            "unrelated inventory should remain unclassified"
        )
        || !require(
            !isLikelyRgbController(unrelated),
            "unrelated inventory should not be marked likely RGB"
        )
        || !require(
            lumacore::hardware::windows::stableProbeId(QStringLiteral("winhid"), QStringLiteral("0B05:19AF \\\\?\\hid#dev"))
                == QStringLiteral("winhid-0b05-19af-hid-dev"),
            "Windows stable probe IDs should be lowercase and path-safe"
        )
        || !require(
            lumacore::hardware::windows::usbVidPidKey(windowsValidatedAsus) == QStringLiteral("0B05:19AF"),
            "Windows VID:PID keys should be normalized to uppercase"
        )
        || !require(
            windowsValidatedSupport.cataloged,
            "validated ASUS identity should be cataloged on Windows"
        )
        || !require(
            windowsValidatedSupport.stage == QStringLiteral("guarded-write-backend"),
            "validated ASUS identity should advertise the shared gated write stage"
        )
        || !require(
            windowsValidatedSupport.writeCapableBackend,
            "validated Windows ASUS identity should advertise the write-capable backend"
        )
        || !require(
            windowsHeuristicSupport.stage == QStringLiteral("heuristic")
                && windowsHeuristicSupport.likelyRgbController,
            "Windows keyword matches should be marked as heuristic likely RGB"
        )
        || !require(
            !emptyWindowsWrite.success
                && emptyWindowsWrite.error.contains(QStringLiteral("HID path is empty")),
            "Windows HID writer should reject empty write targets before transport access"
        )
        || !require(
            inventoryDevices.size() == 2,
            "Windows discovery should keep distinct same-VID/PID HID paths and drop only duplicate IDs"
        )) {
        return 1;
    }

    return 0;
}
