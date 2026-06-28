# Daemon Protocol

`lumacore` talks to `lumacore-daemon` over a local IPC endpoint. Messages are newline-delimited JSON objects with protocol version `1`.
Each request or response is limited to 1 MiB. Oversized requests are rejected by closing the connection. The daemon replaces oversized responses with a matching protocol error; the client closes the connection if a peer sends an oversized response.
The client and server share one internal frame codec for newline extraction, exact-limit handling, empty frames, and JSON parse errors so both sides keep the same protocol boundary.

## Boundary

- `lumacore` is the unprivileged Qt Quick GUI.
- `lumacore-daemon` owns hardware-facing backends and chooses the active backend.
- Linux defaults to `/run/lumacore/lumacore.sock`; Windows uses a versioned, per-user name beginning with `lumacore-daemon-v1-`. Both binaries accept `--socket` to override the endpoint.
- The daemon default backend is `auto`, which aggregates available hardware backends with read-only platform discovery inventory, then falls back to mock.
- On Windows Preview builds, the GUI starts the sibling daemon with `--backend auto --exit-on-disconnect` unless `--no-auto-start-daemon` is supplied. The endpoint is restricted to the current Windows user.

## Request Shape

Requests include:

```json
{"version":1,"id":"1","method":"listDevices","params":{}}
```

Responses include either `ok: true` with `result`, or `ok: false` with `error`.

## Methods

- `hello` / `status` returns daemon version, protocol version, socket path, active backend descriptor, device count, and dry-run state. The daemon answers this handshake regardless of the request's protocol version so the client can detect a version mismatch and surface it instead of failing opaquely; all other methods still require a matching `version`.
- `listDevices` returns the status payload plus device and zone data.
- `previewEffect` returns backend-specific dry-run preview text for one zone/effect.
- `applyEffect` applies a static color or effect through `DeviceManager`, `WriteGate`, and the active backend.
- `updateZone` updates zone metadata such as name and LED count without hardware-write confirmation.
- `confirmWrites` and `revokeWrites` manage per-device in-memory write confirmation for the current daemon session.
- `allOff` runs the backend all-off path through the same confirmation gate.
- `setDryRun` changes daemon dry-run state.
- `activityLogSnapshot` returns formatted activity log lines for the GUI.

Hardware writes are not exposed as raw packet methods. Backends must build approved packets internally and pass the existing permission/write gates.

## Device Snapshots

`listDevices` returns device snapshots with capability, permission, write-confirmation, discovery metadata, and zone data. Device-level `effectSupport` summarizes whether any zone can use each known effect type. Each zone also includes its own `effectSupport` array so the GUI can disable effects, speed controls, and brightness controls that are not valid for that specific header.

Effect support entries use:

```json
{"effectType":1,"name":"Rainbow","supported":true,"speed":false,"brightness":false}
```

Discovery metadata fields such as `discoveryIdentity`, `discoverySupportStage`, `discoverySupportStatus`, `discoverySupportFamily`, `discoverySupportNotes`, `discoveryCataloged`, and `discoveryWriteCapableBackend` are additive protocol-version-1 fields. They describe inventory and support posture only; they do not grant write capability.

The per-zone and discovery metadata fields are additive protocol-version-1 fields; older snapshots without zone-level support fall back to the device-level support in the daemon proxy.
