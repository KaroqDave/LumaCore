// SPDX-License-Identifier: GPL-2.0-or-later

#include "backends/asus/AsusAuraHidDevice.h"

#include "core/EffectsEngine.h"
#include "hardware/linux/AsusAuraHidProtocol.h"

#include <QtGlobal>

#include <utility>

namespace lumacore {

namespace {

const RgbColor kHeader1PreviewColor(30, 84, 214);
const RgbColor kHeader2PreviewColor(34, 211, 238);
const RgbColor kHeader3PreviewColor(124, 92, 255);

} // namespace

AsusAuraHidDevice::AsusAuraHidDevice(
    asus_aura_platform::ProbeDevice device,
    bool configTableVerified,
    hardware::linux::AsusAuraConfigTable configTable,
    QString configSummary,
    QObject* parent
)
    : RgbDevice(
          device.id,
          device.name.isEmpty() ? QStringLiteral("ASUS Aura HID Controller") : device.name,
          device.vendor.isEmpty() ? QStringLiteral("ASUS") : device.vendor,
          RgbDeviceType::Motherboard,
          parent
    )
    , m_device(std::move(device))
    , m_support(asus_aura_platform::discoverySupportInfo(m_device))
    , m_configTableVerified(configTableVerified)
    , m_configTable(std::move(configTable))
    , m_configSummary(std::move(configSummary))
{
    m_configTableVerified = m_configTableVerified
        && hardware::linux::isAsusAuraConfigTableWriteReady(m_configTable);
    setLikelyRgbController(true);
    initializeZones();
}

QString AsusAuraHidDevice::discoveryIdentity() const
{
    return asus_aura_platform::usbVidPidKey(m_device);
}

QString AsusAuraHidDevice::discoverySupportStage() const
{
    return m_support.stage;
}

QString AsusAuraHidDevice::discoverySupportStatus() const
{
    return m_support.status;
}

QString AsusAuraHidDevice::discoverySupportFamily() const
{
    return m_support.family;
}

QString AsusAuraHidDevice::discoverySupportNotes() const
{
    return m_support.notes;
}

bool AsusAuraHidDevice::discoveryCataloged() const
{
    return m_support.cataloged;
}

bool AsusAuraHidDevice::discoveryWriteCapableBackend() const
{
    return m_support.writeCapableBackend;
}

void AsusAuraHidDevice::initializeZones()
{
    const RgbColor previewColors[] = {
        kHeader1PreviewColor,
        kHeader2PreviewColor,
        kHeader3PreviewColor,
    };

    int zoneNumber = 1;
    if (m_configTableVerified && m_configTable.valid) {
        for (const hardware::linux::AsusAuraConfigChannel& channel : m_configTable.channels) {
            if (channel.type != hardware::linux::AsusAuraChannelType::Fixed) {
                continue;
            }

            const int fixedHeaders = qBound(0, channel.headerCount, hardware::linux::kAsusAuraHeaderCount);
            if (fixedHeaders == 0) {
                // The protocol decides whether the headerless fixed channel is
                // exposable as one aggregate zone; zone indices must match it.
                if (hardware::linux::asusAuraFixedExposedZoneCount(m_configTable) > 0) {
                    const RgbColor color = previewColors[(zoneNumber - 1) % 3];
                    mutableZones().append(
                        RgbZone(QStringLiteral("Mainboard LEDs"), RgbZoneType::Motherboard, channel.ledCount, color)
                    );
                    mutableZones().last().setEffect(RgbEffect(RgbEffectType::Static, color, 1.0, 25));
                    ++zoneNumber;
                }
                continue;
            }

            for (int headerIndex = 0; headerIndex < fixedHeaders; ++headerIndex) {
                const RgbColor color = previewColors[(zoneNumber - 1) % 3];
                mutableZones().append(
                    RgbZone(QStringLiteral("Header %1").arg(zoneNumber), RgbZoneType::Motherboard, 1, color)
                );
                mutableZones().last().setEffect(RgbEffect(RgbEffectType::Static, color, 1.0, 25));
                ++zoneNumber;
            }
        }

        int addressableNumber = 1;
        for (const hardware::linux::AsusAuraConfigChannel& channel : m_configTable.channels) {
            if (channel.type != hardware::linux::AsusAuraChannelType::Addressable) {
                continue;
            }

            const RgbColor color = previewColors[(zoneNumber - 1) % 3];
            const int ledCount = qBound(1, channel.ledCount, hardware::linux::kAsusAuraMaxResearchLeds);
            mutableZones().append(
                RgbZone(
                    QStringLiteral("Addressable Header %1").arg(addressableNumber),
                    RgbZoneType::AddressableHeader,
                    ledCount,
                    color
                )
            );
            mutableZones().last().setEffect(RgbEffect(RgbEffectType::Static, color, 1.0, 25));
            ++zoneNumber;
            ++addressableNumber;
        }
    }

    if (!mutableZones().isEmpty()) {
        return;
    }

    mutableZones().append(RgbZone(QStringLiteral("Header 1"), RgbZoneType::Motherboard, 10, kHeader1PreviewColor));
    mutableZones().append(RgbZone(QStringLiteral("Header 2"), RgbZoneType::AddressableHeader, 30, kHeader2PreviewColor));
    mutableZones().append(RgbZone(QStringLiteral("Header 3"), RgbZoneType::AddressableHeader, 30, kHeader3PreviewColor));

    mutableZones()[0].setEffect(RgbEffect(RgbEffectType::Static, kHeader1PreviewColor, 1.0, 25));
    mutableZones()[1].setEffect(RgbEffect(RgbEffectType::Static, kHeader2PreviewColor, 1.0, 25));
    mutableZones()[2].setEffect(RgbEffect(RgbEffectType::Static, kHeader3PreviewColor, 1.0, 25));
}

QString AsusAuraHidDevice::writeDisabledReason() const
{
    return m_configSummary.isEmpty()
        ? QStringLiteral("ASUS Aura HID writes are disabled because no verified config-table response was received.")
        : QStringLiteral("ASUS Aura HID writes are disabled: %1").arg(m_configSummary);
}

bool AsusAuraHidDevice::sendApprovedPacket(
    const hardware::linux::AsusAuraHidProtocolResult& protocol,
    const QString& operation
)
{
    if (!protocol.ok || !protocol.packet.hardwareWriteApproved) {
        m_lastHardwareWriteStatus = !protocol.ok
            ? QStringLiteral("ASUS Aura HID %1 skipped: %2").arg(operation, protocol.error)
            : QStringLiteral("ASUS Aura HID %1 skipped: packet was not hardware-approved.").arg(operation);
        return false;
    }

    const asus_aura_platform::HidWriteResult write = m_writer.writeReports(m_device.path, protocol.packet.reports);
    if (!write.success) {
        m_lastHardwareWriteStatus =
            QStringLiteral("ASUS Aura HID %1 failed on %2: %3").arg(operation, m_device.path, write.error);
        return false;
    }
    m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID %1 sent on interface %2 path %3: %4 report(s), %5 byte(s). %6")
                                    .arg(operation)
                                    .arg(m_device.interfaceNumber)
                                    .arg(m_device.path)
                                    .arg(protocol.packet.reports.size())
                                    .arg(write.bytesWritten)
                                    .arg(protocol.packet.summary);
    return true;
}

bool AsusAuraHidDevice::setZoneStaticColor(int zoneIndex, const RgbColor& color)
{
    return applyZoneEffect(zoneIndex, RgbEffect(RgbEffectType::Static, color, 1.0, 25));
}

bool AsusAuraHidDevice::applyZoneEffect(int zoneIndex, const RgbEffect& effect)
{
    m_lastHardwareWriteStatus.clear();
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID write skipped: invalid zone.");
        return false;
    }

    if (!m_configTableVerified) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID write skipped: %1").arg(writeDisabledReason());
        return false;
    }

    const hardware::linux::AsusAuraHidProtocolResult protocol = effect.isAnimated()
        ? hardware::linux::buildAsusAuraDirectFrameWrite(
              m_configTable,
              zoneIndex,
              EffectsEngine::computeFrame(effect, zones().at(zoneIndex).ledCount(), 0.0),
              true
          )
        : hardware::linux::buildAsusAuraStaticColorWrite(
              m_configTable,
              zoneIndex,
              effect.color(),
              zones().at(zoneIndex).ledCount(),
              effect.brightness()
          );
    if (!sendApprovedPacket(protocol, QStringLiteral("write"))) {
        return false;
    }

    setZoneEffect(zoneIndex, effect);
    if (effect.isAnimated()) {
        const QVector<RgbColor> frame = EffectsEngine::computeFrame(effect, zones().at(zoneIndex).ledCount(), 0.0);
        if (!frame.isEmpty()) {
            mutableZones()[zoneIndex].setLedColors(frame);
        }
    } else {
        mutableZones()[zoneIndex].setColor(effect.color());
    }
    emit zoneChanged(zoneIndex);
    return true;
}

bool AsusAuraHidDevice::applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors)
{
    m_lastHardwareWriteStatus.clear();
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID stream frame skipped: invalid zone.");
        return false;
    }

    if (!m_configTableVerified) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID stream frame skipped: %1").arg(writeDisabledReason());
        return false;
    }

    const hardware::linux::AsusAuraHidProtocolResult protocol =
        hardware::linux::buildAsusAuraDirectFrameWrite(m_configTable, zoneIndex, colors, false);
    if (!sendApprovedPacket(protocol, QStringLiteral("stream frame"))) {
        return false;
    }

    mutableZones()[zoneIndex].setLedColors(colors);
    return true;
}

bool AsusAuraHidDevice::applyAllOff()
{
    m_lastHardwareWriteStatus.clear();
    if (!m_configTableVerified) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID all-off skipped: %1").arg(writeDisabledReason());
        return false;
    }

    const hardware::linux::AsusAuraHidProtocolResult protocol = hardware::linux::buildAsusAuraAllOffWrite(m_configTable);
    if (!sendApprovedPacket(protocol, QStringLiteral("all-off"))) {
        return false;
    }

    applyLocalAllOff();
    return true;
}

void AsusAuraHidDevice::applyLocalAllOff()
{
    const RgbEffect offEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0);
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        setZoneEffect(zoneIndex, offEffect);
        mutableZones()[zoneIndex].setColor(RgbColor(0, 0, 0));
        emit zoneChanged(zoneIndex);
    }
}

bool AsusAuraHidDevice::updateZoneMetadata(int zoneIndex, const QString& name, int ledCount)
{
    if (zoneIndex < 0 || zoneIndex >= zones().size() || ledCount < 1 || ledCount > hardware::linux::kAsusAuraMaxResearchLeds) {
        return false;
    }
    if (zoneIndex < fixedZoneCount() && ledCount != zones().at(zoneIndex).ledCount()) {
        m_lastHardwareWriteStatus = QStringLiteral(
            "ASUS Aura HID fixed-zone LED counts are defined by the verified config table."
        );
        return false;
    }

    return RgbDevice::updateZoneMetadata(zoneIndex, name, ledCount);
}

bool AsusAuraHidDevice::usesLocalFrameRendering() const
{
    return false;
}

bool AsusAuraHidDevice::usesLocalFrameRenderingForEffect(int zoneIndex, const RgbEffect& effect) const
{
    return supportsHostStreamedEffect(zoneIndex, static_cast<int>(effect.type()));
}

QString AsusAuraHidDevice::previewZoneEffectWrite(int zoneIndex, const RgbEffect& effect) const
{
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        return {};
    }

    if (!m_configTableVerified) {
        return writeDisabledReason();
    }

    const hardware::linux::AsusAuraHidProtocolResult protocol = effect.isAnimated()
        ? hardware::linux::buildAsusAuraDirectFrameWrite(
              m_configTable,
              zoneIndex,
              EffectsEngine::computeFrame(effect, zones().at(zoneIndex).ledCount(), 0.0),
              true
          )
        : hardware::linux::buildAsusAuraStaticColorWrite(
              m_configTable,
              zoneIndex,
              effect.color(),
              zones().at(zoneIndex).ledCount(),
              effect.brightness()
          );
    return protocol.ok ? protocol.packet.summary : protocol.error;
}

QString AsusAuraHidDevice::lastHardwareWriteStatus() const
{
    return m_lastHardwareWriteStatus;
}

BackendCapabilities AsusAuraHidDevice::capabilities() const
{
    BackendCapabilities capabilities = BackendCapability::DiscoveryRead;
    if (m_configTableVerified) {
        capabilities |= BackendCapability::ZoneColorWrite | BackendCapability::ZoneEffectWrite;
    }
    return capabilities;
}

PermissionResult AsusAuraHidDevice::checkRuntimePermission(BackendCapability capability) const
{
    if (capability == BackendCapability::DiscoveryRead) {
        return {PermissionStatus::Granted, {}};
    }

    if (capability != BackendCapability::ZoneColorWrite && capability != BackendCapability::ZoneEffectWrite) {
        return {
            PermissionStatus::Denied,
            QStringLiteral("Unsupported ASUS Aura HID operation."),
        };
    }

    if (!m_configTableVerified) {
        return {
            PermissionStatus::Denied,
            writeDisabledReason(),
        };
    }

    return {
        PermissionStatus::RequiresConfirmation,
        QStringLiteral("ASUS Aura HID writes require confirmation before real hardware writes are allowed for this daemon session."),
    };
}

bool AsusAuraHidDevice::supportsEffect(int effectType) const
{
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        if (supportsZoneEffect(zoneIndex, effectType)) {
            return true;
        }
    }
    return false;
}

bool AsusAuraHidDevice::supportsEffectSpeed(int effectType) const
{
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        if (supportsZoneEffectSpeed(zoneIndex, effectType)) {
            return true;
        }
    }
    return false;
}

bool AsusAuraHidDevice::supportsEffectBrightness(int effectType) const
{
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        if (supportsZoneEffectBrightness(zoneIndex, effectType)) {
            return true;
        }
    }
    return false;
}

bool AsusAuraHidDevice::supportsZoneEffect(int zoneIndex, int effectType) const
{
    if (zoneIndex < 0 || zoneIndex >= zones().size()) {
        return false;
    }

    if (effectType == static_cast<int>(RgbEffectType::Static)) {
        return capabilities().testFlag(BackendCapability::ZoneColorWrite);
    }

    if (!capabilities().testFlag(BackendCapability::ZoneEffectWrite)) {
        return false;
    }

    return supportsHostStreamedEffect(zoneIndex, effectType);
}

bool AsusAuraHidDevice::supportsZoneEffectSpeed(int zoneIndex, int effectType) const
{
    return supportsHostStreamedEffect(zoneIndex, effectType);
}

bool AsusAuraHidDevice::supportsZoneEffectBrightness(int zoneIndex, int effectType) const
{
    return supportsZoneEffect(zoneIndex, effectType);
}

bool AsusAuraHidDevice::supportsHostStreamedEffect(int zoneIndex, int effectType) const
{
    return isAddressableZone(zoneIndex)
        && (effectType == static_cast<int>(RgbEffectType::Rainbow)
            || effectType == static_cast<int>(RgbEffectType::Breathing)
            || effectType == static_cast<int>(RgbEffectType::ColorCycle));
}

int AsusAuraHidDevice::fixedZoneCount() const
{
    if (!m_configTableVerified || !m_configTable.valid) {
        return 0;
    }

    return hardware::linux::asusAuraFixedExposedZoneCount(m_configTable);
}

bool AsusAuraHidDevice::isAddressableZone(int zoneIndex) const
{
    return m_configTableVerified
        && m_configTable.valid
        && zoneIndex >= fixedZoneCount()
        && zoneIndex < zones().size();
}

} // namespace lumacore
