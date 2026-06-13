#include "app/Version.h"
#ifdef LUMACORE_HAS_ASUS_AURA_HID
#include "backends/asus/AsusAuraHidBackend.h"
#endif
#ifdef LUMACORE_HAS_LINUX_DISCOVERY
#include "backends/linux/LinuxDiscoveryBackend.h"
#endif
#include "backends/mock/MockBackend.h"
#include "core/DeviceManager.h"
#include "core/RgbBackend.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include <cstdio>
#include <memory>
#include <vector>

namespace {

const QString kAutoBackendId = QStringLiteral("auto");

bool isAsusAuraDiscoveryDuplicate(const lumacore::RgbDevice& device)
{
    const QString text = QStringLiteral("%1 %2").arg(device.vendor(), device.name());
    return device.id().contains(QStringLiteral("0b05-19af"), Qt::CaseInsensitive)
        || (text.contains(QStringLiteral("asus"), Qt::CaseInsensitive)
            && (text.contains(QStringLiteral("aura"), Qt::CaseInsensitive)
                || text.contains(QStringLiteral("led controller"), Qt::CaseInsensitive)));
}

class AutoBackend final : public lumacore::RgbBackend
{
public:
    [[nodiscard]] lumacore::BackendDescriptor descriptor() const override
    {
        return {
            kAutoBackendId,
            QStringLiteral("Auto Backend"),
            QStringLiteral("Aggregates verified ASUS Aura HID control, read-only Linux discovery, and mock fallback."),
            lumacore::BackendCapability::DiscoveryRead
                | lumacore::BackendCapability::ZoneColorWrite
                | lumacore::BackendCapability::ZoneEffectWrite,
        };
    }

    [[nodiscard]] std::vector<std::unique_ptr<lumacore::RgbDevice>> createDevices() const override
    {
        return discoverDevices();
    }

    [[nodiscard]] std::vector<std::unique_ptr<lumacore::RgbDevice>> discoverDevices() const override
    {
        std::vector<std::unique_ptr<lumacore::RgbDevice>> devices;

#ifdef LUMACORE_HAS_ASUS_AURA_HID
        lumacore::AsusAuraHidBackend asusBackend;
        if (asusBackend.probe().isGranted()) {
            for (std::unique_ptr<lumacore::RgbDevice>& device : asusBackend.discoverDevices()) {
                if (device) {
                    device->setBackendId(QStringLiteral("asus-aura-hid"));
                    devices.push_back(std::move(device));
                }
            }
        }
#endif

#ifdef LUMACORE_HAS_LINUX_DISCOVERY
        lumacore::LinuxDiscoveryBackend linuxDiscoveryBackend;
        if (linuxDiscoveryBackend.probe().isGranted()) {
            for (std::unique_ptr<lumacore::RgbDevice>& device : linuxDiscoveryBackend.discoverDevices()) {
                if (!device || isAsusAuraDiscoveryDuplicate(*device)) {
                    continue;
                }
                device->setBackendId(QStringLiteral("linux-discovery"));
                devices.push_back(std::move(device));
            }
        }
#endif

        if (devices.empty()) {
            lumacore::MockBackend mockBackend;
            for (std::unique_ptr<lumacore::RgbDevice>& device : mockBackend.discoverDevices()) {
                if (device) {
                    device->setBackendId(QStringLiteral("mock"));
                    devices.push_back(std::move(device));
                }
            }
        }

        return devices;
    }

    [[nodiscard]] lumacore::PermissionResult probe() const override
    {
        return {lumacore::PermissionStatus::Granted, {}};
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lumacore-daemon"));
    app.setApplicationVersion(lumacore::applicationVersion());
    app.setOrganizationName(QStringLiteral("LumaCore"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Privileged LumaCore RGB backend daemon."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption socketOption(
        QStringList {QStringLiteral("s"), QStringLiteral("socket")},
        QStringLiteral("Unix domain socket path."),
        QStringLiteral("path"),
        lumacore::defaultDaemonSocketPath()
    );
    const QCommandLineOption allowUnprivilegedOption(
        QStringLiteral("allow-unprivileged"),
        QStringLiteral("Allow running the daemon without root privileges for development and tests.")
    );
    const QCommandLineOption backendOption(
        QStringList {QStringLiteral("b"), QStringLiteral("backend")},
        QStringLiteral("Backend id to activate. Use 'auto' to prefer ASUS hardware, then Linux discovery, then mock."),
        QStringLiteral("id"),
        kAutoBackendId
    );
    const QCommandLineOption experimentalWritesOption(
        QStringLiteral("enable-experimental-writes"),
        QStringLiteral("Deprecated compatibility flag; ASUS Aura HID writes are now enabled by config verification and confirmation.")
    );
    parser.addOption(socketOption);
    parser.addOption(allowUnprivilegedOption);
    parser.addOption(backendOption);
    parser.addOption(experimentalWritesOption);
    parser.process(app);
    if (parser.isSet(experimentalWritesOption)) {
        qWarning().noquote()
            << QStringLiteral("--enable-experimental-writes is deprecated and ignored; ASUS writes remain config-verified and confirmation-gated.");
    }

#ifdef Q_OS_LINUX
    if (::geteuid() != 0 && !parser.isSet(allowUnprivilegedOption)) {
        const QByteArray message =
            "lumacore-daemon must run as root. Use --allow-unprivileged only for local development.\n";
        std::fwrite(message.constData(), 1, static_cast<std::size_t>(message.size()), stderr);
        return 1;
    }
#endif

    lumacore::DeviceManager deviceManager;
    deviceManager.registerBackend(std::make_unique<AutoBackend>());
    deviceManager.registerBackend(std::make_unique<lumacore::MockBackend>());
#ifdef LUMACORE_HAS_LINUX_DISCOVERY
    deviceManager.registerBackend(std::make_unique<lumacore::LinuxDiscoveryBackend>());
#endif
#ifdef LUMACORE_HAS_ASUS_AURA_HID
    deviceManager.registerBackend(std::make_unique<lumacore::AsusAuraHidBackend>());
#endif

    const QString requestedBackendId = parser.value(backendOption).trimmed();
    const QString backendId = requestedBackendId.isEmpty() ? kAutoBackendId : requestedBackendId;
    if (!deviceManager.backendRegistry().hasBackend(backendId)) {
        QStringList availableBackendIds = deviceManager.backendRegistry().availableBackendIds();
        availableBackendIds << kAutoBackendId;
        const QString message = QStringLiteral("Unknown backend '%1'. Available backends: %2")
                                    .arg(requestedBackendId, availableBackendIds.join(QStringLiteral(", ")));
        qCritical().noquote() << message;
        std::fprintf(stderr, "%s\n", qPrintable(message));
        return 1;
    }
    if (requestedBackendId == kAutoBackendId) {
        deviceManager.activityLog().info(
            lumacore::LogCategory::Backend,
            QStringLiteral("Auto backend will aggregate ASUS HID control, Linux discovery, and mock fallback.")
        );
    }
    deviceManager.initializeBackends(backendId);

    lumacore::DaemonServer server(&deviceManager);
    QString errorMessage;
    if (!server.listen(parser.value(socketOption), &errorMessage)) {
        qCritical().noquote() << errorMessage;
        std::fprintf(stderr, "%s\n", qPrintable(errorMessage));
        return 1;
    }

    deviceManager.activityLog().info(
        lumacore::LogCategory::System,
        QStringLiteral("lumacore-daemon v%1 listening on %2.")
            .arg(lumacore::applicationVersion(), server.socketPath())
    );

    return QCoreApplication::exec();
}
