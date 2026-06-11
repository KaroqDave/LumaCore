# ![LumaCore icon](assets/icons/lumacore-32.png) LumaCore

**v0.0.9** — Linux-first, open-source RGB control built with C++23, Qt 6, and CMake.

LumaCore is a passion project focused on safe, maintainable RGB control rather than quick hardware hacks. This version is **mock-only** by design: it ships with a Qt Quick desktop app, a core RGB device model, and a simulated ASUS motherboard-style device with three zones. No real hardware access, root permissions, SMBus/I2C writes, USB writes, or hidraw access are used.

![LumaCore Devices view](assets/screenshots/lumacore-devices.png)

## Features (v0.0.9)

- Qt Quick desktop UI with device/zone selection and a color picker
- In-memory RGB model: devices, zones, and LEDs
- Mock backend with one simulated **ASUS TUF X870-PLUS WIFI** device:
  - Header 1 (10 LEDs)
  - Header 2 (30 LEDs)
  - Header 3 (30 LEDs)
- Static color control per zone
- JSON profile save/load under `./profiles`
- Activity log and status bar in the UI
- Embedded multi-resolution application icons

Not yet implemented: breathing/rainbow effects, automated tests, and any real hardware discovery or writes.

## Requirements

- C++23 compiler (GCC or Clang)
- CMake 3.24+
- Ninja or Make
- Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `QuickDialogs2`)

On Arch-based systems:

```sh
sudo pacman -S cmake gcc qt6-base qt6-declarative
```

Package names vary by distribution; install the equivalent Qt 6 development packages for yours.

## Build and run

Configure and build from the repository root:

```sh
cmake -S . -B build
cmake --build build
./build/lumacore
```

Profiles are stored in `./profiles` relative to the **current working directory** when you launch the app. Running `./build/lumacore` from the repo root writes profiles to `profiles/` in the repository (that directory is gitignored).

## Project layout

| Path | Role |
|------|------|
| `app/` | Application startup and Qt/QML wiring |
| `core/` | RGB data model, device ownership, profile save/load |
| `backends/mock/` | Safe simulated hardware backend |
| `ui/` | QML-facing models and `AppController` |
| `ui/qml/` | Qt Quick user interface |
| `assets/icons/` | Application icons (`lumacore.svg` is the source; PNG/ICO sizes are embedded via Qt resources) |

### Data flow

1. `main.cpp` creates `DeviceManager`, UI models, and `AppController`.
2. `DeviceManager` asks `MockBackend` for devices.
3. QML displays devices via `DeviceListModel` and zones via `ZoneListModel`.
4. Color changes go from QML → `AppController` → `DeviceManager` → the mock device.
5. Profile save/load reads and writes JSON under `./profiles`.

`DeviceManager` owns devices with `std::unique_ptr<RgbDevice>`. Devices own zones by value; zones own LEDs by value. QML never owns or directly mutates hardware-shaped objects. `RgbDevice::setZoneStaticColor()` is the only write-like operation today; future hardware backends must keep this boundary and add permission checks, logging, and confirmation before real writes.

## Profile format

Profiles are JSON files in `./profiles`. The filename is the normalized profile name plus `.json` (non-alphanumeric characters become `_`).

```json
{
    "formatVersion": 1,
    "application": "LumaCore",
    "profileName": "default",
    "devices": [
        {
            "id": "mock-asus-tuf-x870-plus-wifi",
            "name": "Mock ASUS TUF X870-PLUS WIFI",
            "vendor": "ASUS",
            "type": "Motherboard",
            "zones": [
                {
                    "name": "Header 1",
                    "type": "Motherboard",
                    "ledCount": 10,
                    "color": "#1E54D6",
                    "rgb": {
                        "red": 64,
                        "green": 128,
                        "blue": 255,
                        "hex": "#1E54D6"
                    }
                }
            ]
        }
    ]
}
```

**Load rules:** devices match by `id`, zones by `name`, colors from the zone `color` hex string. Unknown devices or zones are skipped; invalid colors are skipped and reported in the UI log. The format may evolve while the project remains mock-only.

## Hardware safety

Phase 1 does not touch real hardware. There is no hidapi, libusb, i2c-dev, SMBus, hidraw, udev, root, or administrator code.

**Current guarantees:**

- The GUI talks only to QML models and `AppController`.
- `DeviceManager` owns mock devices only.
- Static color changes update in-memory mock zones and are logged to the console and UI.
- Profiles load through the same mock backend path as the UI.

**Before any real hardware writes:**

- Read-only discovery first
- Structured logging for discovery and write attempts
- Explicit permission detection and clear user-facing errors
- GUI stays unprivileged; privileged work behind a daemon/service boundary
- User confirmation before potentially risky write paths
- Tests for profile and effect behavior on the mock backend first
- No SMBus or Linux `i2c-dev` writes until detection, logging, permissions, confirmation, and a mock-tested control path all exist

## Roadmap

1. **Safe foundation** — mock backend, device model, profiles, basic effects (static, breathing, rainbow), safety documentation *(in progress; v0.0.9 covers mock UI, effects, theming, and profiles)*
2. **Linux integration** — read-only hardware discovery behind feature flags, structured logging, udev/i2c/hidraw/USB permission handling, unprivileged GUI with a daemon boundary
3. **First hardware backends** — read-only ASUS Aura-style motherboard discovery; hidapi/libusb where appropriate; SMBus/i2c only after safety flows exist; mock coverage required before any real write path
4. **Extensibility** — stable backend interface, plugin-style backend loading, profile import/export, device mapping, Windows support without weakening Linux safety boundaries

**Principles:** no direct hardware writes from the GUI, no hidden root requirement, conservative behavior over broad device claims, and a mock backend that stays useful for UI, profile, and effect development.
