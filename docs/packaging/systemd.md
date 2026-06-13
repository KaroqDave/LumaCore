# systemd Service

`packaging/systemd/lumacore-daemon.service` is an example service unit for running the privileged backend daemon.

## Behavior

- The service starts `/usr/bin/lumacore-daemon --socket /run/lumacore/lumacore.sock`.
- The daemon default backend is `auto`: verified ASUS Aura HID first, read-only Linux discovery when useful, then mock.
- The GUI connects to `/run/lumacore/lumacore.sock` by default and should stay unprivileged.
- The daemon still requires root unless started with `--allow-unprivileged`, which is intended only for local development.

## Install Notes

The repository currently provides the service file but not full install targets. Package scripts should install:

- `lumacore` and `lumacore-daemon` into the target binary directory.
- `packaging/systemd/lumacore-daemon.service` into the system unit directory.
- A `lumacore` group when group-based socket access is desired.

The unit sets `Group=lumacore`, `UMask=0007`, and `RuntimeDirectory=lumacore` so the socket directory is created at runtime with group access.

## Backend Overrides

Use a systemd drop-in to force a backend:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/lumacore-daemon --socket /run/lumacore/lumacore.sock --backend asus-aura-hid
```

Use `--backend mock` for packaging smoke tests that should not touch hardware.
