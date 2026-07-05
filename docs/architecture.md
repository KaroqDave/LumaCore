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
Profile scheduling is daemon-owned when the connected daemon advertises `scheduleSupported`:
`DaemonScheduleBridge` mirrors the GUI schedule settings and the scheduled profile's content to
the daemon (`putProfile` + `setSchedule`), where `core/ScheduleService` fires it through the
daemon's own `DeviceManager` — so schedules run while the GUI is closed on a persistent daemon.
`ProfileScheduleRunner` remains the GUI-session fallback and fires exactly as before when the
daemon does not support scheduling; it stands down while the daemon owns the schedule, and
ownership stays sticky across disconnects so both sides never fire the same day.
Diagnostics export is assembled in `AppController` as a sanitized JSON report containing
runtime state, backend/device summaries, profile names, and recent activity without profile contents.
Profile preview, synchronous apply, and asynchronous apply paths share the internal profile
planning helpers so compatibility reports and apply summaries are shaped in one place.

## Module responsibilities

- `core/` owns backend-neutral RGB models, backend registration, write policy, effects, schedule-time normalization, the daemon-side `ScheduleService`, shared profile planning, and the stable `DeviceManager` API.
- `core/ProfileStore` owns profile naming, paths, JSON file I/O, atomic saves, listing, rename, and deletion.
- `core/ProfilePlan` owns internal profile JSON decoding, device/zone matching, effect normalization, preview item construction, and apply report shaping used by both `DeviceManager` and `AppController`.
- `ipc/DaemonProtocol` owns protocol version 1 method names, payload conversion, and snapshot JSON conversion.
- `ipc/DaemonFrameCodec` owns newline-delimited JSON frame extraction, the 1 MiB limit, empty-frame handling, and parse-error conversion shared by `DaemonClient` and `DaemonServer`.
- `ipc/DaemonServer` owns request dispatch after frames are decoded.
- `backends/daemon/` adapts daemon snapshots and calls to the backend-neutral interfaces.
- `backends/auto/` selects and aggregates concrete daemon-side backends.
- `hardware/common/` contains shared discovery catalog helpers; `hardware/asus/` contains the cross-platform ASUS Aura HID protocol serializers; `hardware/linux/` and `hardware/windows/` contain platform probes and HID writer glue. None of these make UI decisions.
- `ui/` exposes stable QML-facing controllers and models. Private UI stores own QSettings-backed device groups and per-zone effect-panel preferences behind the `AppController` facade.

## Stable boundaries

Behavior-preserving refactors must keep these boundaries stable unless a dedicated migration says otherwise:

- `DeviceManager` public methods and Qt signals.
- `AppController` properties, invokables, return defaults, status messages, and signals.
- daemon protocol version 1 method names, fields, error behavior, and newline framing.
- profile filename normalization and format version 1 loading behavior.
- backend IDs, device IDs, device ordering, and permission semantics.
- ASUS Aura packet bytes and hardware-write approval rules.
- internal helper boundaries such as profile planning and daemon framing must not become public API or alter the public reports/protocol they support.

## Migration-only changes

The following should not be bundled into structural refactors:

- replacing the remaining synchronous startup/profile compatibility paths;
- daemon protocol version changes or field removal;
- profile location or schema changes;
- replacing the QML controller surface;
- framework, compiler, CMake, or dependency upgrades;
- new hardware IDs, packet formats, or write capability;
- replacing the daemon/backend architecture.

New hardware work must follow `docs/hardware/contributing-hardware.md`. Read-only discovery,
dry-run previews, and guarded write enablement are separate review stages; write-capable devices
must remain allowlisted, confirmation-gated, and daemon-owned.
