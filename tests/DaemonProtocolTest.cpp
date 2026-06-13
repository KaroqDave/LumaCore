#include "ipc/DaemonProtocol.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

class SnapshotDevice final : public lumacore::RgbDevice
{
public:
    enum class Mode {
        UnverifiedAsus,
        ConfirmableAsus,
    };

    explicit SnapshotDevice(Mode mode)
        : RgbDevice(
              mode == Mode::UnverifiedAsus ? QStringLiteral("asus-unverified") : QStringLiteral("asus-confirmable"),
              QStringLiteral("ASUS Aura HID Controller"),
              QStringLiteral("ASUS"),
              lumacore::RgbDeviceType::Motherboard
          )
        , m_mode(mode)
    {
        setBackendId(QStringLiteral("asus-aura-hid"));
        mutableZones().append(
            lumacore::RgbZone(
                QStringLiteral("Header 1"),
                lumacore::RgbZoneType::Motherboard,
                1,
                lumacore::RgbColor(0, 0, 0)
            )
        );
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        Q_UNUSED(zoneIndex)
        Q_UNUSED(color)
        return false;
    }

    [[nodiscard]] QString lastHardwareWriteStatus() const override
    {
        return m_mode == Mode::UnverifiedAsus
            ? QStringLiteral("ASUS Aura HID write skipped: config-table probe failed.")
            : QString();
    }

    [[nodiscard]] lumacore::BackendCapabilities capabilities() const override
    {
        if (m_mode == Mode::ConfirmableAsus) {
            return lumacore::BackendCapability::DiscoveryRead
                | lumacore::BackendCapability::ZoneColorWrite
                | lumacore::BackendCapability::ZoneEffectWrite;
        }
        return lumacore::BackendCapability::DiscoveryRead;
    }

    [[nodiscard]] lumacore::PermissionResult checkRuntimePermission(lumacore::BackendCapability capability) const override
    {
        if (capability == lumacore::BackendCapability::DiscoveryRead) {
            return {lumacore::PermissionStatus::Granted, {}};
        }
        if (m_mode == Mode::ConfirmableAsus) {
            return {
                lumacore::PermissionStatus::RequiresConfirmation,
                QStringLiteral("ASUS Aura HID writes require confirmation before real hardware writes are allowed."),
            };
        }
        return {
            lumacore::PermissionStatus::Denied,
            QStringLiteral("ASUS Aura HID writes are disabled: ASUS Aura HID config-table probe failed for 0B05:19AF interface=2 path=/dev/hidraw4: timeout."),
        };
    }

private:
    Mode m_mode {Mode::UnverifiedAsus};
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    using namespace lumacore;

    const SnapshotDevice unverified(SnapshotDevice::Mode::UnverifiedAsus);
    const QJsonObject unverifiedJson = deviceToJson(unverified, 0, false);
    const QJsonObject unverifiedPermissions = unverifiedJson.value(QStringLiteral("permissions")).toObject();
    const PermissionResult colorPermission = permissionResultFromJson(
        unverifiedPermissions.value(backendCapabilityToString(BackendCapability::ZoneColorWrite)).toObject()
    );
    if (!require(colorPermission.status == PermissionStatus::Denied, "unverified ASUS color permission should be denied")) {
        return 1;
    }
    if (!require(
            colorPermission.reason.contains(QStringLiteral("config-table probe failed")),
            "unverified ASUS permission reason should survive daemon JSON serialization"
        )) {
        return 1;
    }
    if (!require(unverifiedJson.value(QStringLiteral("readOnly")).toBool(false), "unverified ASUS snapshots should remain read-only")) {
        return 1;
    }
    if (!require(
            unverifiedJson.value(QStringLiteral("lastHardwareWriteStatus")).toString().contains(QStringLiteral("config-table probe failed")),
            "last hardware write status should be serialized"
        )) {
        return 1;
    }

    const SnapshotDevice confirmable(SnapshotDevice::Mode::ConfirmableAsus);
    const QJsonObject confirmedJson = deviceToJson(confirmable, 1, true);
    const PermissionResult confirmedPermission = permissionResultFromJson(
        confirmedJson.value(QStringLiteral("permission")).toObject()
    );
    if (!require(confirmedPermission.status == PermissionStatus::Granted, "confirmed daemon snapshots should upgrade write permission to granted")) {
        return 1;
    }
    if (!require(
            !confirmedJson.value(QStringLiteral("writeRequiresConfirmation")).toBool(true),
            "confirmed daemon snapshots should not still request confirmation"
        )) {
        return 1;
    }

    return 0;
}
