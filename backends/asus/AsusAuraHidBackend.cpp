// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/asus/AsusAuraHidBackend.h"

#include "backends/asus/AsusAuraHidDevice.h"
#include "backends/asus/AsusAuraHidPlatform.h"
#include "hardware/linux/AsusAuraHidProtocol.h"

#include <QHash>
#include <QVector>

namespace lumacore {

namespace {

struct AuraCandidate {
    asus_aura_platform::ProbeDevice device;
    bool configVerified {false};
    hardware::linux::AsusAuraConfigTable config;
    QString configSummary;
};

bool textMatchesAsusAura(const QString& vendor, const QString& product)
{
    const bool vendorMatches = vendor.contains(QStringLiteral("asus"), Qt::CaseInsensitive)
        || vendor.contains(QStringLiteral("asustek"), Qt::CaseInsensitive);
    const bool productMatches = product.contains(QStringLiteral("aura"), Qt::CaseInsensitive)
        || product.contains(QStringLiteral("led controller"), Qt::CaseInsensitive);
    return vendorMatches && productMatches;
}

bool isResearchedAuraUsbDevice(const asus_aura_platform::ProbeDevice& device)
{
    return device.source == QStringLiteral("hidapi")
        && hardware::linux::isAsusAuraUsbVendor(device.vendorId)
        && hardware::linux::isAsusAuraResearchedUsbProduct(device.productId);
}

bool isValidatedAuraLedController(const asus_aura_platform::ProbeDevice& device)
{
    return isResearchedAuraUsbDevice(device)
        && hardware::linux::isAsusAuraWriteValidatedProduct(device.productId);
}

bool isAllowedAuraDevice(const asus_aura_platform::ProbeDevice& device)
{
    return isValidatedAuraLedController(device)
        && !device.path.trimmed().isEmpty();
}

QString physicalControllerKey(const asus_aura_platform::ProbeDevice& device)
{
    return asus_aura_platform::usbVidPidKey(device);
}

QString interfaceSummary(const asus_aura_platform::ProbeDevice& device)
{
    return QStringLiteral("%1 interface=%2 path=%3")
        .arg(physicalControllerKey(device))
        .arg(device.interfaceNumber)
        .arg(device.path.isEmpty() ? QStringLiteral("<empty>") : device.path);
}

int deviceSelectionScore(const asus_aura_platform::ProbeDevice& device)
{
    int score = 0;
    if (textMatchesAsusAura(device.vendor, device.name)) {
        score += 100;
    }
    if (device.name.contains(QStringLiteral("led controller"), Qt::CaseInsensitive)) {
        score += 20;
    }
    if (device.name.contains(QStringLiteral("aura"), Qt::CaseInsensitive)) {
        score += 10;
    }
    if (device.usagePage >= 0xFF00) {
        score += 10;
    }
    return score;
}

bool isBetterAuraInterface(
    const asus_aura_platform::ProbeDevice& candidate,
    const asus_aura_platform::ProbeDevice& current
)
{
    const int candidateScore = deviceSelectionScore(candidate);
    const int currentScore = deviceSelectionScore(current);
    if (candidateScore != currentScore) {
        return candidateScore > currentScore;
    }

    if (candidate.interfaceNumber != current.interfaceNumber) {
        if (candidate.interfaceNumber < 0 || current.interfaceNumber < 0) {
            return candidate.interfaceNumber >= 0;
        }
        return candidate.interfaceNumber < current.interfaceNumber;
    }

    return candidate.path < current.path;
}

AuraCandidate probeAuraCandidate(const asus_aura_platform::ProbeDevice& device)
{
    AuraCandidate candidate;
    candidate.device = device;

    const asus_aura_platform::HidDeviceWriter writer;
    const asus_aura_platform::HidRequestResult response = writer.writeReportReadResponse(
        device.path,
        hardware::linux::buildAsusAuraConfigTableRequest(),
        hardware::linux::kAsusAuraResearchReportLength + 1,
        750
    );
    if (!response.success) {
        candidate.configSummary = QStringLiteral("ASUS Aura HID config-table probe failed for %1: %2")
                                      .arg(interfaceSummary(device))
                                      .arg(response.error);
        return candidate;
    }

    const hardware::linux::AsusAuraConfigTable config = hardware::linux::parseAsusAuraConfigTableResponse(response.response);
    candidate.configVerified = hardware::linux::isAsusAuraConfigTableWriteReady(config);
    candidate.config = config;
    candidate.configSummary = candidate.configVerified
        ? QStringLiteral("ASUS Aura HID validated %1. %2").arg(interfaceSummary(device), config.summary)
        : QStringLiteral("ASUS Aura HID config-table response invalid for %1: %2")
              .arg(interfaceSummary(device))
              .arg(config.error);
    return candidate;
}

bool isBetterAuraCandidate(const AuraCandidate& candidate, const AuraCandidate& current)
{
    if (candidate.configVerified != current.configVerified) {
        return candidate.configVerified;
    }

    return AsusAuraHidBackend::prefersProbeDevice(candidate.device, current.device);
}

} // namespace

BackendDescriptor AsusAuraHidBackend::descriptor() const
{
    return {
        QStringLiteral("asus-aura-hid"),
        QStringLiteral("ASUS Aura HID"),
        QStringLiteral(
            "Standard ASUS Aura USB HID support for validated target %1. Researched related IDs are %2 but are not write-enabled until verified. Static color and native hardware effects are confirmation-gated."
        )
            .arg(
                hardware::linux::asusAuraDeviceKey(),
                hardware::linux::asusAuraResearchedDeviceKeys().join(QStringLiteral(", "))
            ),
        BackendCapability::DiscoveryRead | BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite,
    };
}

bool AsusAuraHidBackend::acceptsProbeDevice(const asus_aura_platform::ProbeDevice& device)
{
    return isAllowedAuraDevice(device);
}

bool AsusAuraHidBackend::prefersProbeDevice(
    const asus_aura_platform::ProbeDevice& candidate,
    const asus_aura_platform::ProbeDevice& current
)
{
    return isBetterAuraInterface(candidate, current);
}

std::vector<std::unique_ptr<RgbDevice>> AsusAuraHidBackend::discoverDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    const asus_aura_platform::ProbeResult result = asus_aura_platform::probeHidapiDevices();
    if (!result.providerAvailable) {
        return devices;
    }

    QVector<AuraCandidate> selectedDevices;
    QHash<QString, int> selectedIndexByKey;
    for (const asus_aura_platform::ProbeDevice& device : result.devices) {
        if (!acceptsProbeDevice(device)) {
            continue;
        }

        AuraCandidate selectedDevice = probeAuraCandidate(device);
        const QString key = physicalControllerKey(selectedDevice.device);
        selectedDevice.device.id = asus_aura_platform::stableProbeId(QStringLiteral("asus-aura-hid"), key);
        if (!selectedDevice.configSummary.isEmpty()) {
            selectedDevice.device.details = selectedDevice.device.details.isEmpty()
                ? selectedDevice.configSummary
                : QStringLiteral("%1, %2").arg(selectedDevice.device.details, selectedDevice.configSummary);
        }

        const auto existingIndex = selectedIndexByKey.constFind(key);
        if (existingIndex == selectedIndexByKey.constEnd()) {
            selectedIndexByKey.insert(key, selectedDevices.size());
            selectedDevices.append(selectedDevice);
            continue;
        }

        AuraCandidate& currentDevice = selectedDevices[*existingIndex];
        if (isBetterAuraCandidate(selectedDevice, currentDevice)) {
            currentDevice = selectedDevice;
        }
    }

    devices.reserve(static_cast<std::size_t>(selectedDevices.size()));
    for (const AuraCandidate& candidate : selectedDevices) {
        auto auraDevice = std::make_unique<AsusAuraHidDevice>(
            candidate.device,
            candidate.configVerified,
            candidate.config,
            candidate.configSummary
        );
        auraDevice->setBackendId(descriptor().id);
        devices.push_back(std::move(auraDevice));
    }

    return devices;
}

PermissionResult AsusAuraHidBackend::probe() const
{
    if (!asus_aura_platform::hidapiProbeAvailable()) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("ASUS Aura HID backend requires hidapi."),
        };
    }

    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
