<div align="center">
  <h1>
    <img src="assets/icons/lumacore-128.png" alt="LumaCore icon" width="64" height="64" align="center" />
    LumaCore
  </h1>

  <p>
    <img src="https://img.shields.io/github/actions/workflow/status/KaroqDave/LumaCore/build.yml?branch=main&label=build%20verified" alt="Build verified" />
    <img src="https://img.shields.io/github/actions/workflow/status/KaroqDave/LumaCore/tests.yml?branch=main&label=tests%20verified" alt="Tests verified" />
  </p>
</div>

**v1.1.8.1** - Linux-first RGB control with Windows read-only HID discovery, portable diagnostics export, improved Windows preview packaging, and automatic daemon startup through the `auto` backend, built with C++23, Qt 6, and CMake. Licensed under GPL-2.0-or-later.

LumaCore is a safe, daemon-backed RGB controller for Linux desktops. The Qt Quick GUI stays unprivileged and talks to `lumacore-daemon` over a local IPC endpoint; hardware-facing code runs behind backend capability checks, dry-run logging, and explicit write confirmation.

The Windows 10/11 x64 preview preserves the daemon architecture: it launches the bundled daemon automatically, can expose read-only HID inventory when hidapi is available, and can use the same gated ASUS Aura HID write backend for the validated controller target. Windows starts dry-run-first and still requires explicit confirmation before real writes.

![LumaCore Devices view](assets/screenshots/lumacore-devices.png)

![LumaCore ASUS hardware write confirmation](assets/screenshots/lumacore-devices-collapsed-confirmation.png)

![LumaCore active backend dialog](assets/screenshots/lumacore-backend-dialog.png)

Light and collapsed-sidebar screenshots are also kept in `assets/screenshots/`.

## Current Capabilities

- Qt Quick desktop UI with a compact collapsible navigation rail, denser Devices workflow, Profiles, Settings, Activities, backend status, and an About dialog.
- Global controls for applying one effect or brightness level across all compatible zones or saved device groups, plus All Off for every writable device or a selected group, with partial-result reporting.
- In-memory RGB model for devices, zones, LEDs, profiles, static colors, rainbow, breathing, and color-cycle effects.
- GUI-to-daemon boundary through `backends/daemon/`, `ipc/`, and `lumacore-daemon`.
- Non-blocking interactive daemon requests with correlated responses, cancellation, bounded reconnect backoff, automatic device refresh, stable selection restoration, and manual Retry/Rescan controls.
- Default daemon `auto` backend that prefers verified ASUS Aura HID control on Linux, adds read-only platform discovery inventory when available, and falls back to the mock backend.
- Mock backend with a simulated ASUS TUF X870-PLUS WIFI motherboard for UI, profile, and effect development.
- Optional Linux read-only discovery through compiled providers such as hidapi, libusb, and i2c-dev adapter metadata, with cataloged RGB-controller research identities and conservative heuristic classification.
- Optional Windows read-only HID discovery through hidapi, with cataloged RGB-controller research identities and conservative heuristic classification. Windows discovery itself is inventory-only; the separate ASUS Aura HID backend owns the guarded write path.
- ASUS Aura USB HID backend for the allowlisted `0B05:19AF` controller, including config-table-derived zones, static/direct color writes, native color-cycle/rainbow effects on addressable headers, and All Off.
- Profile save, load, rename, confirmed overwrite/delete, JSON import/export, compatibility reporting, partial-result summaries, and persisted active-profile selection with atomic writes and legacy color-only profile compatibility.
- Portable Auto/Light/Dark themes, animation and dry-run preferences, start-minimized and active-profile launch behavior, daily in-app profile scheduling, opt-in close-to-tray behavior, and an enabled-by-default Windows VRR flicker workaround.
- Activity log with structured severity/category entries and console mirroring.
- Backend capability dialog showing daemon connection/version details, active backend, dry-run state, supported operations, Retry/Rescan, and sanitized diagnostics export.

## Safety Model

- `lumacore` refuses to run as root; `lumacore-daemon` requires root by default on Linux.
- Hardware writes are never exposed as raw packet methods to the GUI.
- Linux and Windows discovery backends are read-only and do not perform RGB writes.
- Dry-run mode logs write intent and backend-specific previews without applying changes.
- ASUS Aura writes require an allowlisted device, a verified `EC B0`/`EC 30` config-table response, dry-run off, approved packet builders, and per-device confirmation for the current daemon session.
- Confirmation is held in memory and is cleared when the daemon restarts or the backend is reinitialized.
- SMBus/I2C writes, generic hidraw writes, persistent hardware configuration, firmware writes, and unconfirmed ASUS writes are intentionally out of scope.

See `docs/hardware/asus-aura-hid.md` for the ASUS protocol notes, licensing boundary, and validation checklist.
New hardware contributions must follow `docs/hardware/contributing-hardware.md`, which separates research notes, read-only discovery, dry-run previews, and guarded write enablement.

## Requirements

- C++23 compiler, such as GCC or Clang
- CMake 3.24+
- Ninja or Make
- Qt 6.5+ with `Core`, `Gui`, `Network`, `Qml`, `Quick`, `QuickControls2`, `QuickDialogs2`, and `Widgets`
- Optional for Linux discovery and ASUS Aura HID builds: `pkg-config`, `hidapi`, and/or `libusb`
- Optional for Windows HID discovery: a hidapi package discoverable by CMake, such as `hidapi::hidapi`, `hidapi::winapi`, or pkg-config `hidapi`; otherwise the bundled HIDAPI 0.15.0 Windows backend is used by default.

On Arch-based systems:

```sh
sudo pacman -S cmake gcc qt6-base qt6-declarative hidapi libusb
```

Package names vary by distribution; install the equivalent Qt 6 development packages for yours.

## Build and Run

Configure and build from the repository root:

```sh
cmake -S . -B build
cmake --build build
```

Start the daemon, then launch the GUI from another terminal:

```sh
sudo ./build/lumacore-daemon
./build/lumacore
```

Both binaries use `/run/lumacore/lumacore.sock` by default. Override it with `--socket` when running without the packaged service setup.

For an unprivileged mock-only development session:

```sh
./build/lumacore-daemon --allow-unprivileged --backend mock --socket /tmp/lumacore.sock
./build/lumacore --socket /tmp/lumacore.sock
```

## VS Code Development

The repository includes CMake presets, clangd configuration, QML language-server configuration, and recommended VS Code extensions. The Linux/WSL presets write `build/compile_commands.json`, which clangd uses for the real Qt include paths, compiler flags, and generated headers. (The native Windows preset configures into `build-windows/`, leaving the Linux/WSL tree in `build/` intact for editor tooling.)

On Linux or in WSL, install the dependencies listed above, open the repository in the WSL VS Code window, and select the `linux-debug` configure preset:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

WSL is recommended when working on the Linux discovery and ASUS HID write backends because those sources are intentionally excluded from native Windows builds. Native Windows builds include only the Windows read-only HID discovery path.

For a Qt Online Installer setup on Windows, copy `CMakeUserPresets.json.example` to the ignored `CMakeUserPresets.json`, update `QT_ROOT_DIR` and `MINGW_ROOT`, and select the resulting `windows-local` preset in CMake Tools:

```powershell
Copy-Item CMakeUserPresets.json.example CMakeUserPresets.json
cmake --preset windows-local
cmake --build --preset windows-local
ctest --preset windows-local
```

The local preset adds Qt and MinGW to the build environment so clangd can query the compiler for its target and standard-library include paths.

Use the release preset for portable Windows preview packages:

```powershell
cmake --preset windows-local-release
cmake --build --preset windows-local-release
ctest --preset windows-local-release
.\packaging\windows\package.ps1 -BuildDir .\build-windows-release
```

On Windows, launching `lumacore.exe` automatically starts the sibling `lumacore-daemon.exe` with the `auto` backend. It uses read-only HID discovery when compiled with hidapi and falls back to mock devices otherwise. Pass `--no-auto-start-daemon` to disable this behavior. The default local endpoint is a versioned, per-user name beginning with `lumacore-daemon-v1-`.

See [docs/windows-preview.md](docs/windows-preview.md) for end-user instructions and preview limitations.

Stage Linux install artifacts for packaging with:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
DESTDIR="$PWD/dist/linux-stage" cmake --install build --prefix /usr
```

The install target stages `lumacore`, `lumacore-daemon`, the desktop entry, hicolor icons, and the configured systemd unit. Package post-install scripts should create the `lumacore` group when group-based daemon socket access is desired, then reload systemd and enable/start `lumacore-daemon.service` according to the distribution's policy.

Run the same QML analysis enforced by CI with:

```sh
cmake --build build --target all_qmllint
```

The native Windows preset configures into `build-windows/` while the Linux/WSL presets use `build/`, so the same checkout can hold both platform build trees at once without reconfiguring or deleting either. clangd reads `build/compile_commands.json`, so the Linux/WSL tree drives editor tooling.

Backend overrides:

```sh
sudo ./build/lumacore-daemon --backend linux-discovery
sudo ./build/lumacore-daemon --backend asus-aura-hid
```

The daemon accepts `--backend auto`, `mock`, `linux-discovery`, `windows-discovery`, or `asus-aura-hid` when those backends are built. `auto` is the default.

## CMake Options

- `LUMACORE_ENABLE_LINUX_DISCOVERY` builds daemon-only Linux read-only discovery on supported systems.
- `LUMACORE_ENABLE_WINDOWS_DISCOVERY` builds daemon-only Windows read-only HID discovery on supported systems.
- `LUMACORE_ENABLE_HIDAPI` enables hidapi discovery when available.
- `LUMACORE_USE_BUNDLED_HIDAPI` uses the vendored HIDAPI Windows backend when no system hidapi package is found. It defaults to `ON` for Windows and `OFF` elsewhere.
- `LUMACORE_ENABLE_LIBUSB` enables libusb discovery when available.
- `LUMACORE_ENABLE_I2C_DEV` enables optional read-only i2c-dev adapter metadata discovery.
- `LUMACORE_ENABLE_ASUS_AURA_HID` builds the ASUS Aura USB HID backend with config-verified, confirmation-gated writes on supported Linux and Windows builds when hidapi is available. It uses the platform HID discovery and writer transport.
- `LUMACORE_ENABLE_WARNINGS` enables project compiler warnings for first-party targets.
- `LUMACORE_WARNINGS_AS_ERRORS` treats project compiler warnings as errors.
- `LUMACORE_ENABLE_SANITIZERS` enables AddressSanitizer and UndefinedBehaviorSanitizer for supported GNU/Clang Linux builds.
- `LUMACORE_INSTALL_SYSTEMD_UNIT` controls whether Linux installs stage the systemd unit.
- `LUMACORE_SYSTEMD_UNIT_DIR` overrides the systemd unit install destination.

## Tests

Run all configured CTest targets after building:

```sh
ctest --test-dir build --output-on-failure
```

Run the Linux sanitizer configuration on Linux or WSL:

```sh
cmake --preset linux-sanitizer
cmake --build --preset linux-sanitizer
ctest --preset linux-sanitizer
```

Current tests cover write confirmation and gating, profile persistence, profile apply/preview
reports, daemon frame limits and snapshots, auto-backend deduplication, option parsing, schedule
settings, application controllers, daemon launch behavior, and, when the ASUS backend is built,
the ASUS Aura HID configuration parser and protocol serializer.

## Project Layout

- `app/` - application startup, version helper, and Qt/QML wiring.
- `core/` - RGB model, effects, profile storage and planning, schedule-time parsing, activity log, backend registry, permission gate, and write gate.
- `backends/auto/` - daemon-side hardware/discovery aggregation with mock fallback.
- `backends/mock/` - safe simulated hardware backend.
- `backends/daemon/` - GUI-facing backend that talks to `lumacore-daemon`.
- `backends/linux/` - daemon-only read-only Linux discovery backend.
- `backends/windows/` - daemon-only read-only Windows HID discovery backend.
- `backends/asus/` - ASUS Aura USB HID backend.
- `daemon/` - privileged daemon entry point and backend registration.
- `hardware/linux/` - Linux provider probes, HID writer, and ASUS Aura protocol helpers.
- `hardware/windows/` - Windows HID provider probes and discovery catalog helpers.
- `ipc/` - local daemon protocol, shared frame codec, client, and server.
- `ui/` and `ui/qml/` - QML-facing controllers, models, private UI preference stores, and Qt Quick UI.
- `docs/` - architecture, daemon protocol, refactor parity, release verification, hardware notes, Windows preview, and systemd packaging notes.
- `packaging/` - desktop entry, systemd unit template, and Windows preview packaging.
- `tests/` - focused unit tests.
- `assets/` - icons and screenshots. After editing `assets/icons/lumacore.svg`, run `python scripts/generate-icons.py` (requires PySide6 and Pillow).

## Profiles

Profiles are JSON files stored under LumaCore's application data root. Portable and local builds use `data/profiles` beside the running executable; installed Linux builds use the platform data location for the GUI and `/var/lib/lumacore` for the root daemon service. App settings and Qt/QML caches live under the same root, so the Windows preview does not write registry settings or AppData state for normal operation. On first use, LumaCore migrates JSON profiles from the legacy `./profiles` directory when the new directory does not yet exist. Saves use `QSaveFile` for atomic replacement.

Devices match by `id`; zones match by `name` with their stored index as a fallback. Profiles restore zone names, LED counts, colors, effect types, speed, and brightness. Legacy color-only zones still load. Unknown devices or zones are skipped, invalid colors are reported in the activity log, and a profile that matches no available zones is rejected. Synchronous startup/scheduled applies and asynchronous interactive applies share the same internal profile planner so report keys, skip counts, details, and preview item shapes stay aligned.

## Documentation

- `docs/daemon/protocol.md` documents the newline-delimited JSON socket protocol.
- `docs/architecture.md` documents runtime boundaries and stable APIs.
- `docs/refactor-parity.md` is the behavior-preservation checklist for structural changes.
- `docs/release-verification.md` documents repeatable build, test, lint, sanitizer, install-staging, and Windows preview checks.
- `docs/windows-preview.md` documents Windows preview behavior and limitations.
- `docs/hardware/asus-aura-hid.md` documents the guarded ASUS Aura HID support and protocol research boundaries.
- `docs/hardware/discovery-catalog.md` documents read-only discovery classification stages and cataloged research identities.
- `docs/hardware/contributing-hardware.md` documents the staged workflow and PR checklist for new hardware support.
- `docs/packaging/systemd.md` documents Linux install staging, the systemd service, and backend overrides.

## Current Gaps

- Automated coverage is still focused; broader mock-backend and end-to-end UI integration work remains beyond the current CTest, QML lint, warning, sanitizer, and package-staging checks.
- Startup and scheduled profile application still use the synchronous compatibility path; direct interactive device operations and GUI profile loads are asynchronous.
- Profile scheduling currently runs inside the GUI session; daemon/systemd background scheduling is not implemented.
- ASUS support is intentionally limited to the allowlisted controller until more owned-hardware validation follows the hardware contribution workflow.
- Profile validation is minimal.
- Native installers and Linux distribution packages are not implemented; CMake install targets stage Linux package inputs and Windows packaging produces a portable preview ZIP.

## License

LumaCore is free software licensed under the GNU General Public License version 2.0 or later (`GPL-2.0-or-later`).

See `LICENSE` for the full terms.
