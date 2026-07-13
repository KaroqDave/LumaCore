# Daemon Protocol

`lumacore` talks to `lumacore-daemon` over a local IPC endpoint. Messages are newline-delimited JSON objects with protocol version `1`.
Each request or response is limited to 1 MiB. Oversized requests are rejected by closing the connection. The daemon replaces oversized responses with a matching protocol error; the client closes the connection if a peer sends an oversized response.
The client and server share one internal frame codec for newline extraction, exact-limit handling, empty frames, and JSON parse errors so both sides keep the same protocol boundary.

## Boundary

- `lumacore` is the unprivileged Qt Quick GUI.
- `lumacore-daemon` owns hardware-facing backends and chooses the active backend.
- In the GUI, `daemon` is the transport/proxy backend. The daemon's selected hardware backend
  remains visible separately as the effective backend identity, for example `daemon -> auto`.
- Linux defaults to `/run/lumacore/lumacore.sock`; Windows uses a versioned, per-user name beginning with `lumacore-daemon-v1-`. Both binaries accept `--socket` to override the endpoint.
- The daemon default backend is `auto`, which aggregates available hardware backends with read-only platform discovery inventory, then falls back to mock.
- On Windows builds, the GUI starts the sibling daemon with `--backend auto --exit-on-disconnect` unless `--no-auto-start-daemon` is supplied. The endpoint is restricted to the current Windows user.
- The daemon accepts one active GUI client at a time. A new connection replaces the previous client socket.

## Request Shape

Requests include:

```json
{"version":1,"id":"1","method":"listDevices","params":{}}
```

Responses include either `ok: true` with `result`, or `ok: false` with `error`.

## Methods

- `hello` / `status` returns daemon version, protocol version, socket path, active backend descriptor, device count, dry-run state, the additive `discoveryComplete` readiness flag, and the additive `scheduleSupported` flag that advertises the daemon-side schedule methods. The daemon answers this handshake regardless of the request's protocol version so the client can detect a version mismatch and surface it instead of failing opaquely; all other methods still require a matching `version`.
- `listDevices` returns the status payload plus device and zone data. Clients that set the additive `acceptIncompleteDiscovery: true` parameter receive `discoveryComplete: false`, `deviceCount: 0`, and an empty `devices` array while discovery is in progress, then poll for the final inventory. Calls that omit the parameter are held until discovery completes, preserving the one-shot inventory behavior expected by older protocol-v1 GUIs.
- `previewEffect` returns backend-specific dry-run preview text for one zone/effect.
- `applyEffect` applies a static color or effect through `DeviceManager`, `WriteGate`, and the active backend.
- `updateZone` updates zone metadata such as name and LED count without hardware-write confirmation.
- `confirmWrites` and `revokeWrites` manage per-device in-memory write confirmation for the current daemon session.
- `allOff` runs the backend all-off path through the same confirmation gate.
- `paintZoneFrame` streams one host-computed LED frame through `DeviceManager::paintZoneFrame` and the active backend. Write requests require the same explicit `dryRunEnabled` guard as `applyEffect` and `allOff`. Animated GUI effects set `clientStreamsFrames` on `applyEffect` so the daemon primes hardware while the GUI streams frames over `paintZoneFrame`.
- `setDryRun` changes daemon dry-run state and echoes the daemon's effective dry-run state. The GUI tracks both its local setting and the daemon-reported value so diagnostics can expose synchronization problems instead of inferring daemon state from the GUI. Write requests (`applyEffect`, `allOff`, `paintZoneFrame`) always carry the GUI's own dry-run expectation, never the daemon-reported value, so the daemon's synchronization guard can refuse writes when the two states drift apart.
- `activityLogSnapshot` returns formatted activity log lines for the GUI.
- `getSchedule` returns the daemon's persisted profile schedule as `enabled`, `profileName`, `time` (`HH:mm`), and `profileAvailable` (whether the daemon's own profile store contains the scheduled profile). Additive protocol-version-1 method.
- `setSchedule` stores the daemon-side schedule from `enabled`, `profileName`, and `time`, then echoes the normalized effective state in the `getSchedule` shape. Profile names are trimmed and invalid times normalize to `18:00`; enabling without a profile name is rejected with `Schedule requires a profile name.` and leaves the stored schedule unchanged. This is configuration, not a hardware write, so it carries no dry-run guard. Additive protocol-version-1 method.
- `putProfile` saves a format-version-1 profile JSON object into the daemon's own profile store under the normalized `profileName`, so the GUI can mirror the scheduled profile into a store the daemon can read (the GUI and a root daemon use different data roots). Missing or non-object payloads and unknown `formatVersion` values are rejected; identical re-pushes are accepted without rewriting. Additive protocol-version-1 method.

The GUI mirrors its schedule to the daemon when support is advertised: it pushes the scheduled profile via `putProfile` followed by `setSchedule`, and re-pushes when the schedule settings or the scheduled profile's content change. Scheduled applies run inside the daemon through the same permission, confirmation, and dry-run gates as interactive writes; unconfirmed hardware zones are skipped with a logged reason.

Hardware writes are not exposed as raw packet methods. Backends must build approved packets internally and pass the existing permission/write gates.

Device-specific preview, metadata, confirmation, and write methods are rejected with `Device discovery is still in progress.` until `discoveryComplete` becomes true. Status, inventory, dry-run synchronization, activity-log, profile-storage, and schedule-configuration methods remain available during discovery. Clients that connect to an older daemon where `discoveryComplete` is absent treat discovery as complete.

## Device Snapshots

`listDevices` returns device snapshots with capability, permission, write-confirmation, discovery metadata, and zone data. Device-level `effectSupport` summarizes whether any zone can use each known effect type. Each zone also includes its own `effectSupport` array so the GUI can disable effects, speed controls, and brightness controls that are not valid for that specific header.

The per-capability `permissions` map always carries the raw permission results; the per-session confirmation is reported separately via `writeConfirmed`, and clients apply the `RequiresConfirmation` → `Granted` upgrade dynamically from that flag. This keeps the confirmation requirement recoverable when a confirmation is revoked after the snapshot was taken. Only the aggregate `permission` field is serialized pre-upgraded, as the effective status for display fallbacks.

Effect support entries use:

```json
{"effectType":1,"name":"Rainbow","supported":true,"speed":false,"brightness":false}
```

Discovery metadata fields such as `discoveryIdentity`, `discoverySupportStage`, `discoverySupportStatus`, `discoverySupportFamily`, `discoverySupportNotes`, `discoveryCataloged`, and `discoveryWriteCapableBackend` are additive protocol-version-1 fields. They describe inventory and support posture only; they do not grant write capability.

The per-zone and discovery metadata fields are additive protocol-version-1 fields; older snapshots without zone-level support fall back to the device-level support in the daemon proxy.
