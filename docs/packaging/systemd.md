# systemd Service

`packaging/systemd/lumacore-daemon.service` is an example service unit for running the privileged backend daemon.
`packaging/systemd/lumacore-daemon.service.in` is the CMake install template; install builds substitute the configured binary directory into `ExecStart`.

## Behavior

- The service starts `/usr/bin/lumacore-daemon --socket /run/lumacore/lumacore.sock`.
- The daemon default backend is `auto`: verified ASUS Aura HID first, read-only Linux discovery when useful, then mock.
- The GUI connects to `/run/lumacore/lumacore.sock` by default and should stay unprivileged.
- The daemon still requires root unless started with `--allow-unprivileged`, which is intended only for local development.

## Install Notes

The CMake install target stages the files a Linux package needs:

- `lumacore` and `lumacore-daemon` into `${CMAKE_INSTALL_BINDIR}`.
- `packaging/lumacore.desktop.in` as `share/applications/lumacore.desktop`.
- LumaCore icons into the hicolor icon theme.
- The configured `lumacore-daemon.service` into `LUMACORE_SYSTEMD_UNIT_DIR` when `LUMACORE_INSTALL_SYSTEMD_UNIT` is enabled.
- A `lumacore` group when group-based socket access is desired.

The unit sets `Group=lumacore`, `UMask=0007`, and `RuntimeDirectory=lumacore` so the socket directory is created at runtime with group access.

For a package staging tree:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
DESTDIR="$PWD/dist/linux-stage" cmake --install build --prefix /usr
```

Override the systemd destination when the target distribution expects a different unit directory:

```sh
cmake -S . -B build -DLUMACORE_SYSTEMD_UNIT_DIR=lib/systemd/system
```

## Backend Overrides

Use a systemd drop-in to force a backend:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/lumacore-daemon --socket /run/lumacore/lumacore.sock --backend asus-aura-hid
```

Use `--backend mock` for packaging smoke tests that should not touch hardware.
