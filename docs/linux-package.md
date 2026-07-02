# LumaCore Linux Package

LumaCore ships a Linux x64 release archive named `LumaCore-Linux-x64.tar.gz`.
The archive is a staged `/usr` install tree, not an AppImage or distro package.
Target systems must provide Qt 6 runtime libraries and normal Linux shared-library dependencies.

## Download and Inspect

```sh
tar -xzf LumaCore-Linux-x64.tar.gz
cd LumaCore-Linux-x64
find usr -maxdepth 3 -type f | sort
```

Expected contents include:

- `usr/bin/lumacore`
- `usr/bin/lumacore-daemon`
- `usr/share/applications/lumacore.desktop`
- `usr/share/icons/hicolor/.../lumacore.*`
- `usr/lib/systemd/system/lumacore-daemon.service`

## Safe Mock Session

Use this path first. It does not require installation and does not touch hardware:

```sh
./usr/bin/lumacore-daemon --allow-unprivileged --backend mock --socket /tmp/lumacore.sock
```

In another terminal:

```sh
./usr/bin/lumacore --socket /tmp/lumacore.sock
```

## Install-Style Test

On a machine you control, copy the staged tree into `/usr`:

```sh
sudo cp -a usr/* /usr/
sudo systemctl daemon-reload
```

For a manual mock run after copying:

```sh
lumacore-daemon --allow-unprivileged --backend mock --socket /tmp/lumacore.sock
```

In another terminal:

```sh
lumacore --socket /tmp/lumacore.sock
```

For the packaged service path:

```sh
sudo groupadd --system --force lumacore
sudo systemctl enable --now lumacore-daemon.service
lumacore
```

If the GUI cannot connect to the service socket, add your user to the `lumacore` group and log out and back in:

```sh
sudo usermod -aG lumacore "$USER"
```

## Hardware Notes

- The daemon requires root by default on Linux because hardware-facing backends may need privileged device access.
- `--allow-unprivileged` is intended for mock and local development sessions.
- The `auto` backend prefers verified ASUS Aura HID support, adds read-only Linux discovery when useful, then falls back to mock.
- Real hardware writes should stay dry-run-first and limited to owned, validated hardware.
