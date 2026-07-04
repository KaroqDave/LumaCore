// SPDX-License-Identifier: GPL-2.0-or-later

// Edge-case coverage for the DaemonServer write-dispatch methods
// (applyEffect, previewEffect, updateZone, allOff, paintZoneFrame, confirm/
// revoke). These methods read loosely-typed JSON params and delegate to
// DeviceManager, whose bounds guards (deviceAt / deviceForZone / setZoneEffect-
// Colors) are only exercised with valid indices elsewhere. A GUI never sends an
// out-of-range deviceIndex, a zoneIndex past the end, an omitted index, or a
// wrong-typed effect/colors value, but the daemon must still answer such
// requests with a structured success:false result rather than crashing or
// reading out of bounds. This drives the real in-process server through a
// DaemonClient with a functional one-zone device so "one past the end" is a
// genuine boundary.

#include "core/DeviceManager.h"
#include "core/RgbColor.h"
#include "core/RgbDevice.h"
#include "core/RgbEffect.h"
#include "core/RgbZone.h"
#include "ipc/DaemonClient.h"
#include "ipc/DaemonProtocol.h"
#include "ipc/DaemonServer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <memory>

namespace {

using namespace lumacore;

int g_failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << "FAIL:" << message;
        ++g_failures;
    }
}

QString testSocketName(const QString& suffix)
{
    return QStringLiteral("lumacore-write-dispatch-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(suffix);
}

// A minimal but functional device: one addressable zone, writes granted outright
// (no confirmation) so dry-run-off writes to valid targets actually apply and
// out-of-range writes fail on the bounds guard rather than on a permission gate.
class DispatchDevice final : public RgbDevice {
public:
    DispatchDevice()
        : RgbDevice(
              QStringLiteral("dispatch-device"),
              QStringLiteral("Dispatch Device"),
              QStringLiteral("Test"),
              RgbDeviceType::Controller
          )
    {
        mutableZones().append(RgbZone(
            QStringLiteral("Zone 1"),
            RgbZoneType::AddressableHeader,
            4,
            RgbColor(1, 2, 3)
        ));
    }

    [[nodiscard]] bool setZoneStaticColor(int zoneIndex, const RgbColor& color) override
    {
        return applyZoneEffect(zoneIndex, RgbEffect(RgbEffectType::Static, color));
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return BackendCapability::DiscoveryRead
            | BackendCapability::ZoneColorWrite
            | BackendCapability::ZoneEffectWrite;
    }

    [[nodiscard]] PermissionResult checkRuntimePermission(BackendCapability) const override
    {
        return {PermissionStatus::Granted, {}};
    }
};

// Result accessors: the transport layer reports ok; the write payload carries the
// semantic success flag and error text.
bool payloadSuccess(const DaemonCallResult& result)
{
    return result.result.value(QStringLiteral("success")).toBool(false);
}

QString payloadError(const DaemonCallResult& result)
{
    return result.result.value(QStringLiteral("error")).toString();
}

bool payloadHasSuccessFlag(const DaemonCallResult& result)
{
    return result.result.value(QStringLiteral("success")).isBool();
}

QJsonObject staticEffectJson()
{
    return RgbEffect(RgbEffectType::Static, RgbColor(30, 84, 214)).toJson();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    DeviceManager manager;
    std::vector<std::unique_ptr<RgbDevice>> devices;
    devices.push_back(std::make_unique<DispatchDevice>());
    manager.replaceDevices(std::move(devices));

    DaemonServer server(&manager);
    const QString serverName = testSocketName(QStringLiteral("dispatch"));
    QString listenError;
    const bool listening = server.listen(serverName, &listenError);
    check(listening, "write-dispatch server should listen");
    if (!listening) {
        qCritical().noquote() << listenError;
        return 1;
    }
    DaemonClient client(serverName);

    const QString apply = daemonMethodName(DaemonMethod::ApplyEffect);
    const QString preview = daemonMethodName(DaemonMethod::PreviewEffect);
    const QString updateZone = daemonMethodName(DaemonMethod::UpdateZone);
    const QString allOff = daemonMethodName(DaemonMethod::AllOff);
    const QString paint = daemonMethodName(DaemonMethod::PaintZoneFrame);
    const QString confirm = daemonMethodName(DaemonMethod::ConfirmWrites);
    const QString revoke = daemonMethodName(DaemonMethod::RevokeWrites);

    // ---- Phase 1: dry-run enabled. Write requests carry a matching dry-run
    // expectation so the dry-run guard passes and the bounds/param checks are
    // what actually decide the outcome. ----
    manager.setDryRunEnabled(true);

    const DaemonCallResult applyValid = client.call(apply, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("effect"), staticEffectJson()},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(applyValid.ok && payloadSuccess(applyValid), "control: a valid dry-run applyEffect should succeed");

    const DaemonCallResult applyBadDevice = client.call(apply, {
        {QStringLiteral("deviceIndex"), 99},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("effect"), staticEffectJson()},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(applyBadDevice.ok && !payloadSuccess(applyBadDevice), "applyEffect on an out-of-range device index must fail cleanly");

    const DaemonCallResult applyBadZone = client.call(apply, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 99},
        {QStringLiteral("effect"), staticEffectJson()},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(applyBadZone.ok && !payloadSuccess(applyBadZone), "applyEffect on an out-of-range zone index must fail cleanly");

    const DaemonCallResult applyNegative = client.call(apply, {
        {QStringLiteral("deviceIndex"), -5},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("effect"), staticEffectJson()},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(applyNegative.ok && !payloadSuccess(applyNegative), "applyEffect on a negative device index must fail cleanly");

    // Omitted indices default to -1 and must be rejected, not treated as device 0.
    const DaemonCallResult applyMissingIndices = client.call(apply, {
        {QStringLiteral("effect"), staticEffectJson()},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(applyMissingIndices.ok && !payloadSuccess(applyMissingIndices), "applyEffect with omitted indices must be rejected");

    // A wrong-typed deviceIndex (string) also defaults to -1 rather than crashing.
    const DaemonCallResult applyStringIndex = client.call(apply, {
        {QStringLiteral("deviceIndex"), QStringLiteral("not-a-number")},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("effect"), staticEffectJson()},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(applyStringIndex.ok && !payloadSuccess(applyStringIndex), "applyEffect with a non-numeric device index must be rejected");

    // A wrong-typed effect (string instead of object) must degrade to a default
    // effect and produce a structured result, never crash the daemon.
    const DaemonCallResult applyBadEffect = client.call(apply, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("effect"), QStringLiteral("this-should-be-an-object")},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(
        applyBadEffect.ok && payloadHasSuccessFlag(applyBadEffect),
        "applyEffect with a wrong-typed effect must return a structured result without crashing"
    );

    // previewEffect uses the same target validation.
    const DaemonCallResult previewBad = client.call(preview, {
        {QStringLiteral("deviceIndex"), 99},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("effect"), staticEffectJson()},
    }, 3000);
    check(
        previewBad.ok && !payloadSuccess(previewBad)
            && payloadError(previewBad).contains(QStringLiteral("Invalid device or zone")),
        "previewEffect on an out-of-range target must report an invalid device or zone"
    );

    const DaemonCallResult previewValid = client.call(preview, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("effect"), staticEffectJson()},
    }, 3000);
    check(previewValid.ok && payloadSuccess(previewValid), "control: previewEffect on a valid target should succeed");

    // updateZone has no dry-run guard; it validates target, name, and LED count.
    const DaemonCallResult updateBadZone = client.call(updateZone, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 99},
        {QStringLiteral("name"), QStringLiteral("Renamed")},
        {QStringLiteral("ledCount"), 4},
    }, 3000);
    check(
        updateBadZone.ok && !payloadSuccess(updateBadZone)
            && payloadError(updateBadZone).contains(QStringLiteral("Could not update selected zone")),
        "updateZone on an out-of-range zone must report an invalid selection"
    );

    const DaemonCallResult updateEmptyName = client.call(updateZone, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("name"), QStringLiteral("   ")},
        {QStringLiteral("ledCount"), 4},
    }, 3000);
    check(
        updateEmptyName.ok && !payloadSuccess(updateEmptyName)
            && payloadError(updateEmptyName).contains(QStringLiteral("cannot be empty")),
        "updateZone with a whitespace-only name must be rejected"
    );

    // ledCount omitted defaults to 0, which is below the minimum.
    const DaemonCallResult updateZeroLeds = client.call(updateZone, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("name"), QStringLiteral("Renamed")},
    }, 3000);
    check(
        updateZeroLeds.ok && !payloadSuccess(updateZeroLeds)
            && payloadError(updateZeroLeds).contains(QStringLiteral("at least 1")),
        "updateZone with an omitted (zero) LED count must be rejected"
    );

    const DaemonCallResult updateValid = client.call(updateZone, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("name"), QStringLiteral("Renamed")},
        {QStringLiteral("ledCount"), 8},
    }, 3000);
    check(updateValid.ok && payloadSuccess(updateValid), "control: a valid updateZone should succeed");

    const DaemonCallResult allOffBad = client.call(allOff, {
        {QStringLiteral("deviceIndex"), 99},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(
        allOffBad.ok && !payloadSuccess(allOffBad)
            && payloadError(allOffBad).contains(QStringLiteral("Could not turn off")),
        "allOff on an out-of-range device must fail cleanly"
    );

    const DaemonCallResult allOffValid = client.call(allOff, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("dryRunEnabled"), true},
    }, 3000);
    check(allOffValid.ok && payloadSuccess(allOffValid), "control: a valid dry-run allOff should succeed");

    const DaemonCallResult confirmBad = client.call(confirm, {{QStringLiteral("deviceIndex"), 99}}, 3000);
    const DaemonCallResult revokeBad = client.call(revoke, {{QStringLiteral("deviceIndex"), 99}}, 3000);
    check(confirmBad.ok && !payloadSuccess(confirmBad), "confirmWrites on an out-of-range device must fail cleanly");
    check(revokeBad.ok && !payloadSuccess(revokeBad), "revokeWrites on an out-of-range device must fail cleanly");

    // ---- Phase 2: dry-run disabled, so paintZoneFrame's own target validation
    // decides the outcome (under dry-run every frame is accepted by design). ----
    manager.setDryRunEnabled(false);

    const QVector<RgbColor> frame {RgbColor(10, 20, 30), RgbColor(40, 50, 60), RgbColor(70, 80, 90), RgbColor(100, 110, 120)};

    const DaemonCallResult paintValid = client.call(paint, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("colors"), colorsToJson(frame)},
        {QStringLiteral("dryRunEnabled"), false},
    }, 3000);
    check(paintValid.ok && payloadSuccess(paintValid), "control: a valid paintZoneFrame should apply");

    const DaemonCallResult paintBadDevice = client.call(paint, {
        {QStringLiteral("deviceIndex"), 99},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("colors"), colorsToJson(frame)},
        {QStringLiteral("dryRunEnabled"), false},
    }, 3000);
    check(paintBadDevice.ok && !payloadSuccess(paintBadDevice), "paintZoneFrame on an out-of-range device must be dropped");

    const DaemonCallResult paintBadZone = client.call(paint, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 99},
        {QStringLiteral("colors"), colorsToJson(frame)},
        {QStringLiteral("dryRunEnabled"), false},
    }, 3000);
    check(paintBadZone.ok && !payloadSuccess(paintBadZone), "paintZoneFrame on an out-of-range zone must be dropped");

    // A wrong-typed colors value decodes to an empty frame, which is rejected
    // rather than crashing the frame path.
    const DaemonCallResult paintBadColors = client.call(paint, {
        {QStringLiteral("deviceIndex"), 0},
        {QStringLiteral("zoneIndex"), 0},
        {QStringLiteral("colors"), QStringLiteral("not-an-array")},
        {QStringLiteral("dryRunEnabled"), false},
    }, 3000);
    check(
        paintBadColors.ok && !payloadSuccess(paintBadColors),
        "paintZoneFrame with a wrong-typed colors value must be dropped without crashing"
    );

    client.disconnectFromDaemon();
    server.close();

    if (g_failures != 0) {
        qCritical().noquote() << g_failures << "write-dispatch check(s) failed";
        return 1;
    }
    return 0;
}
