#include "backends/asus/AsusAuraHidBackend.h"

#include "backends/asus/AsusAuraHidDevice.h"
#include "hardware/linux/AsusAuraHidProtocol.h"
#include "hardware/linux/HidDeviceWriter.h"
#include "hardware/linux/HidapiProbe.h"

#include <QHash>
#include <QVector>

namespace lumacore {

namespace {

struct AuraCandidate {
    hardware::linux::ProbeDevice device;
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

bool isResearchedAuraUsbDevice(const hardware::linux::ProbeDevice& device)
{
    return device.source == QStringLiteral("hidapi")
        && hardware::linux::isAsusAuraUsbVendor(device.vendorId)
        && hardware::linux::isAsusAuraResearchedUsbProduct(device.productId);
}

bool isValidatedAuraLedController(const hardware::linux::ProbeDevice& device)
{
    return isResearchedAuraUsbDevice(device)
        && hardware::linux::isAsusAuraWriteValidatedProduct(device.productId);
}

bool isAllowedAuraDevice(const hardware::linux::ProbeDevice& device)
{
    return isValidatedAuraLedController(device)
        && device.interfaceNumber >= 0
        && !device.path.trimmed().isEmpty();
}

QString physicalControllerKey(const hardware::linux::ProbeDevice& device)
{
    return hardware::linux::usbVidPidKey(device);
}

QString interfaceSummary(const hardware::linux::ProbeDevice& device)
{
    return QStringLiteral("%1 interface=%2 path=%3")
        .arg(physicalControllerKey(device))
        .arg(device.interfaceNumber)
        .arg(device.path.isEmpty() ? QStringLiteral("<empty>") : device.path);
}

int deviceSelectionScore(const hardware::linux::ProbeDevice& device)
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

bool isBetterAuraInterface(const hardware::linux::ProbeDevice& candidate, const hardware::linux::ProbeDevice& current)
{
    const int candidateScore = deviceSelectionScore(candidate);
    const int currentScore = deviceSelectionScore(current);
    if (candidateScore != currentScore) {
        return candidateScore > currentScore;
    }

    if (candidate.interfaceNumber != current.interfaceNumber) {
        return candidate.interfaceNumber < current.interfaceNumber;
    }

    return candidate.path < current.path;
}

AuraCandidate probeAuraCandidate(const hardware::linux::ProbeDevice& device)
{
    AuraCandidate candidate;
    candidate.device = device;

    const hardware::linux::HidDeviceWriter writer;
    const hardware::linux::HidRequestResult response = writer.writeReportReadResponse(
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

    return isBetterAuraInterface(candidate.device, current.device);
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

std::vector<std::unique_ptr<RgbDevice>> AsusAuraHidBackend::createDevices() const
{
    return discoverDevices();
}

std::vector<std::unique_ptr<RgbDevice>> AsusAuraHidBackend::discoverDevices() const
{
    std::vector<std::unique_ptr<RgbDevice>> devices;
    const hardware::linux::ProbeResult result = hardware::linux::probeHidapiDevices();
    if (!result.providerAvailable) {
        return devices;
    }

    QVector<AuraCandidate> selectedDevices;
    QHash<QString, int> selectedIndexByKey;
    for (const hardware::linux::ProbeDevice& device : result.devices) {
        if (!isAllowedAuraDevice(device)) {
            continue;
        }

        AuraCandidate selectedDevice = probeAuraCandidate(device);
        const QString key = physicalControllerKey(selectedDevice.device);
        selectedDevice.device.id = hardware::linux::stableProbeId(QStringLiteral("asus-aura-hid"), key);
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
    if (!hardware::linux::hidapiProbeAvailable()) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("ASUS Aura HID backend requires hidapi."),
        };
    }

    return {PermissionStatus::Granted, {}};
}

} // namespace lumacore
