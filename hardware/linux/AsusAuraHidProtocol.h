#pragma once

#include "core/RgbColor.h"
#include "core/RgbEffect.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace lumacore::hardware::linux {

inline constexpr quint16 kAsusAuraVendorId = 0x0B05;
inline constexpr quint16 kAsusAuraLedControllerProductId = 0x19AF;
inline constexpr quint16 kAsusAuraAddressableHeaderProductId = 0x18F3;
inline constexpr quint16 kAsusAuraTerminalProductId = 0x1939;
inline constexpr int kAsusAuraResearchReportLength = 65;
inline constexpr int kAsusAuraMaxResearchLeds = 120;
inline constexpr int kAsusAuraHeaderCount = 3;

enum class AsusAuraHidPacketKind {
    ResearchPreview,
    StaticColorWrite,
    NativeEffectWrite,
    AllOffWrite,
};

enum class AsusAuraChannelType {
    Fixed,
    Addressable,
};

struct AsusAuraHidPacket {
    QByteArray report;
    QVector<QByteArray> reports;
    QString summary;
    QString provenance;
    AsusAuraHidPacketKind kind {AsusAuraHidPacketKind::ResearchPreview};
    bool hardwareWriteApproved {false};
};

struct AsusAuraConfigChannel {
    AsusAuraChannelType type {AsusAuraChannelType::Fixed};
    int effectChannel {0};
    int directChannel {0};
    int ledCount {0};
    int headerCount {0};
};

struct AsusAuraHidProtocolResult {
    bool ok {false};
    AsusAuraHidPacket packet;
    QString error;
};

struct AsusAuraConfigTable {
    bool valid {false};
    int addressableHeaderCount {0};
    int mainboardLedCount {0};
    int rgbHeaderCount {0};
    QByteArray response;
    QByteArray table;
    QVector<AsusAuraConfigChannel> channels;
    QString summary;
    QString error;
};

[[nodiscard]] QString asusAuraDeviceKey();
[[nodiscard]] QString asusAuraDeviceKey(quint16 productId);
[[nodiscard]] QStringList asusAuraResearchedDeviceKeys();
[[nodiscard]] bool isAsusAuraUsbVendor(const QString& vendorId);
[[nodiscard]] bool isAsusAuraResearchedUsbProduct(const QString& productId);
[[nodiscard]] bool isAsusAuraWriteValidatedProduct(const QString& productId);
[[nodiscard]] QByteArray buildAsusAuraConfigTableRequest();
[[nodiscard]] AsusAuraConfigTable parseAsusAuraConfigTableResponse(const QByteArray& response);
[[nodiscard]] AsusAuraHidProtocolResult buildAsusAuraStaticColorPreview(
    int zoneIndex,
    const RgbColor& color,
    int ledCount,
    int brightness
);
[[nodiscard]] AsusAuraHidProtocolResult buildAsusAuraStaticColorWrite(
    int zoneIndex,
    const RgbColor& color,
    int ledCount,
    int brightness
);
[[nodiscard]] AsusAuraHidProtocolResult buildAsusAuraStaticColorWrite(
    const AsusAuraConfigTable& config,
    int zoneIndex,
    const RgbColor& color,
    int ledCount,
    int brightness
);
[[nodiscard]] AsusAuraHidProtocolResult buildAsusAuraNativeEffectWrite(
    const AsusAuraConfigTable& config,
    int zoneIndex,
    const RgbEffect& effect,
    int ledCount
);
[[nodiscard]] AsusAuraHidProtocolResult buildAsusAuraAllOffWrite();
[[nodiscard]] AsusAuraHidProtocolResult buildAsusAuraAllOffWrite(const AsusAuraConfigTable& config);

} // namespace lumacore::hardware::linux
