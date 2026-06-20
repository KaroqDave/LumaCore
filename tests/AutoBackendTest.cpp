#include "backends/auto/AutoBackend.h"
#include "core/RgbDevice.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSet>

#include <utility>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

class IdentityDevice final : public lumacore::RgbDevice
{
public:
    explicit IdentityDevice(QString identity)
        : RgbDevice(
              QStringLiteral("identity-device"),
              QStringLiteral("ASUS Aura LED Controller"),
              QStringLiteral("ASUS"),
              lumacore::RgbDeviceType::Controller
          )
        , m_identity(std::move(identity))
    {
    }

    [[nodiscard]] QString discoveryIdentity() const override
    {
        return m_identity;
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const lumacore::RgbColor& color) override
    {
        Q_UNUSED(zoneIndex)
        Q_UNUSED(color)
        return false;
    }

private:
    QString m_identity;
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    const QSet<QString> representedIdentities {QStringLiteral("0B05:19AF")};
    const IdentityDevice represented(QStringLiteral("0B05:19AF"));
    const IdentityDevice unsupported(QStringLiteral("0B05:1A2B"));
    const IdentityDevice unidentified {QString()};

    if (!require(
            lumacore::AutoBackend::isRepresentedDiscoveryDevice(
                represented,
                representedIdentities
            ),
            "an exact discovery identity should be deduplicated"
        )
        || !require(
            !lumacore::AutoBackend::isRepresentedDiscoveryDevice(
                unsupported,
                representedIdentities
            ),
            "an unsupported ASUS identity should remain visible"
        )
        || !require(
            !lumacore::AutoBackend::isRepresentedDiscoveryDevice(
                unidentified,
                representedIdentities
            ),
            "a device without a discovery identity should remain visible"
        )) {
        return 1;
    }

    return 0;
}
