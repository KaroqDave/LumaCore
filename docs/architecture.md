# Architecture

LumaCore is split across an unprivileged Qt Quick application and a hardware-facing daemon.
The public boundaries are intentionally narrower than the implementation modules behind them.

## Runtime flow

1. `lumacore` starts or connects to `lumacore-daemon`.
2. The GUI registers `DaemonBackend` with its local `DeviceManager`.
3. `DaemonBackend` obtains device snapshots through daemon protocol version 1.
4. QML reads device data through `DeviceTreeModel` and invokes actions through `AppController`.
5. Proxy `DaemonRgbDevice` objects translate interactive write operations into correlated asynchronous daemon calls.
6. The daemon's `DeviceManager` applies permission checks, confirmation state, dry-run policy, and backend writes.

`DaemonClient` supports concurrent request correlation, cancellation, timeouts, bounded reconnect,
and a synchronous compatibility wrapper used by startup discovery and bulk profile application.
After reconnect, the GUI requests a fresh device snapshot and replaces the proxy-device set atomically.
When the desktop exposes a system tray, `TrayController` owns the native tray icon and mediates
Show/Hide, explicit Quit, and the opt-in close-to-tray window lifecycle.
`ProfileScheduleRunner` is a GUI-session timer that applies one persisted profile schedule
through the normal `AppController` profile path; it does not move scheduling into the daemon.
Diagnostics export is assembled in `AppController` as a sanitized JSON report containing
runtime state, backend/device summaries, profile names, and recent activity without profile contents.

## Module responsibilities

- `core/` owns backend-neutral RGB models, backend registration, write policy, effects, and the stable `DeviceManager` API.
- `core/ProfileStore` owns profile naming, paths, JSON file I/O, atomic saves, listing, rename, and deletion. `DeviceManager` still owns matching a loaded profile to live devices.
- `ipc/DaemonProtocol` owns protocol version 1 names, framing, and JSON conversion.
- `ipc/DaemonServer` owns local-socket framing and request dispatch.
- `backends/daemon/` adapts daemon snapshots and calls to the backend-neutral interfaces.
- `backends/auto/` selects and aggregates concrete daemon-side backends.
- `hardware/linux/` contains provider probes and protocol serializers; it does not make UI decisions.
- `ui/` exposes stable QML-facing controllers and models.

## Stable boundaries

Behavior-preserving refactors must keep these boundaries stable unless a dedicated migration says otherwise:

- `DeviceManager` public methods and Qt signals.
- `AppController` properties, invokables, return defaults, status messages, and signals.
- daemon protocol version 1 method names, fields, error behavior, and newline framing.
- profile filename normalization and format version 1 loading behavior.
- backend IDs, device IDs, device ordering, and permission semantics.
- ASUS Aura packet bytes and hardware-write approval rules.

## Migration-only changes

The following should not be bundled into structural refactors:

- replacing the remaining synchronous startup/profile compatibility paths;
- moving profile scheduling into a background daemon or operating-system timer;
- daemon protocol version changes or field removal;
- profile location or schema changes;
- replacing the QML controller surface;
- framework, compiler, CMake, or dependency upgrades;
- new hardware IDs, packet formats, or write capability;
- replacing the daemon/backend architecture.

New hardware work must follow `docs/hardware/contributing-hardware.md`. Read-only discovery,
dry-run previews, and guarded write enablement are separate review stages; write-capable devices
must remain allowlisted, confirmation-gated, and daemon-owned.
