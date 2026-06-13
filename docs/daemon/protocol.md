# Daemon Protocol

`lumacore` talks to `lumacore-daemon` over a local Unix domain socket. Messages are newline-delimited JSON objects with protocol version `1`.

## Boundary

- `lumacore` is the unprivileged Qt Quick GUI.
- `lumacore-daemon` owns hardware-facing backends and chooses the active backend.
- The default socket path is `/run/lumacore/lumacore.sock`; both binaries accept `--socket` to override it.
- The daemon default backend is `auto`, which aggregates verified ASUS Aura HID control with read-only Linux discovery inventory, then falls back to mock.

## Request Shape

Requests include:

```json
{"version":1,"id":"1","method":"listDevices","params":{}}
```

Responses include either `ok: true` with `result`, or `ok: false` with `error`.

## Methods

- `hello` / `status` returns daemon version, socket path, active backend descriptor, device count, and dry-run state.
- `listDevices` returns the status payload plus device and zone data.
- `previewEffect` returns backend-specific dry-run preview text for one zone/effect.
- `applyEffect` applies a static color or effect through `DeviceManager`, `WriteGate`, and the active backend.
- `updateZone` updates zone metadata such as name and LED count without hardware-write confirmation.
- `confirmWrites` and `revokeWrites` manage per-device in-memory write confirmation for the current daemon session.
- `allOff` runs the backend all-off path through the same confirmation gate.
- `setDryRun` changes daemon dry-run state.
- `activityLogSnapshot` returns formatted activity log lines for the GUI.

Hardware writes are not exposed as raw packet methods. Backends must build approved packets internally and pass the existing permission/write gates.
