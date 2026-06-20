#include "backends/asus/AsusAuraHidDevice.h"

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
    hardware::linux::ProbeDevice device,
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
    , m_configTableVerified(configTableVerified)
    , m_configTable(std::move(configTable))
    , m_configSummary(std::move(configSummary))
{
    setLikelyRgbController(true);
    initializeZones();
}

QString AsusAuraHidDevice::discoveryIdentity() const
{
    return hardware::linux::usbVidPidKey(m_device);
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
        ? hardware::linux::buildAsusAuraNativeEffectWrite(
              m_configTable,
              zoneIndex,
              effect,
              zones().at(zoneIndex).ledCount()
          )
        : hardware::linux::buildAsusAuraStaticColorWrite(
              m_configTable,
              zoneIndex,
              effect.color(),
              zones().at(zoneIndex).ledCount(),
              effect.brightness()
          );
    if (!protocol.ok || !protocol.packet.hardwareWriteApproved) {
        m_lastHardwareWriteStatus = !protocol.ok
            ? QStringLiteral("ASUS Aura HID write skipped: %1").arg(protocol.error)
            : QStringLiteral("ASUS Aura HID write skipped: packet was not hardware-approved.");
        return false;
    }

    const hardware::linux::HidWriteResult write = m_writer.writeReports(m_device.path, protocol.packet.reports);
    if (!write.success) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID write failed on %1: %2").arg(m_device.path, write.error);
        return false;
    }
    m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID write sent on interface %1 path %2: %3 report(s), %4 byte(s). %5")
                                    .arg(m_device.interfaceNumber)
                                    .arg(m_device.path)
                                    .arg(protocol.packet.reports.size())
                                    .arg(write.bytesWritten)
                                    .arg(protocol.packet.summary);

    setZoneEffect(zoneIndex, effect);
    mutableZones()[zoneIndex].setColor(effect.color());
    emit zoneChanged(zoneIndex);
    return true;
}

bool AsusAuraHidDevice::applyZoneFrame(int zoneIndex, const QVector<RgbColor>& colors)
{
    Q_UNUSED(zoneIndex)
    Q_UNUSED(colors)
    return false;
}

bool AsusAuraHidDevice::applyAllOff()
{
    m_lastHardwareWriteStatus.clear();
    if (!m_configTableVerified) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID all-off skipped: %1").arg(writeDisabledReason());
        return false;
    }

    const hardware::linux::AsusAuraHidProtocolResult protocol = hardware::linux::buildAsusAuraAllOffWrite(m_configTable);
    if (!protocol.ok || !protocol.packet.hardwareWriteApproved) {
        m_lastHardwareWriteStatus = !protocol.ok
            ? QStringLiteral("ASUS Aura HID all-off skipped: %1").arg(protocol.error)
            : QStringLiteral("ASUS Aura HID all-off skipped: packet was not hardware-approved.");
        return false;
    }

    const hardware::linux::HidWriteResult write = m_writer.writeReports(m_device.path, protocol.packet.reports);
    if (!write.success) {
        m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID all-off failed on %1: %2").arg(m_device.path, write.error);
        return false;
    }
    m_lastHardwareWriteStatus = QStringLiteral("ASUS Aura HID all-off sent on interface %1 path %2: %3 report(s), %4 byte(s). %5")
                                    .arg(m_device.interfaceNumber)
                                    .arg(m_device.path)
                                    .arg(protocol.packet.reports.size())
                                    .arg(write.bytesWritten)
                                    .arg(protocol.packet.summary);

    const RgbEffect offEffect(RgbEffectType::Static, RgbColor(0, 0, 0), 1.0, 0);
    for (int zoneIndex = 0; zoneIndex < zones().size(); ++zoneIndex) {
        setZoneEffect(zoneIndex, offEffect);
        mutableZones()[zoneIndex].setColor(RgbColor(0, 0, 0));
        emit zoneChanged(zoneIndex);
    }
    return true;
}

bool AsusAuraHidDevice::updateZoneMetadata(int zoneIndex, const QString& name, int ledCount)
{
    if (zoneIndex < 0 || zoneIndex >= zones().size() || ledCount < 1 || ledCount > hardware::linux::kAsusAuraMaxResearchLeds) {
        return false;
    }

    return RgbDevice::updateZoneMetadata(zoneIndex, name, ledCount);
}

bool AsusAuraHidDevice::usesLocalFrameRendering() const
{
    return false;
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
        ? hardware::linux::buildAsusAuraNativeEffectWrite(m_configTable, zoneIndex, effect, zones().at(zoneIndex).ledCount())
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

} // namespace lumacore
