# Repository Guidelines

## Project Structure & Module Organization

LumaCore is a C++23/Qt 6.5+ RGB controller split between an unprivileged Qt Quick GUI and hardware-facing daemon.

- `app/` contains startup, command-line options, daemon launch/tray helpers, scheduling, and QML hosting.
- `core/` owns RGB models, profile storage/planning, schedules, effects, activity logging, backend registration, and write policy.
- `daemon/` and `ipc/` implement the daemon process, protocol v1 methods, local socket client/server, and shared frame codec.
- `backends/` contains `mock`, `auto`, `daemon`, Linux/Windows discovery, and ASUS Aura HID backends.
- `ui/` and `ui/qml/` contain QML-facing controllers, models, private preference stores, and Qt Quick components.
- `hardware/linux/` contains Linux read-only probes, HID writer glue, and shared ASUS protocol serializers.
- `hardware/windows/` contains Windows HID discovery probes, HID writer glue, and discovery catalog helpers.
- `tests/` contains CTest executables; `docs/` records architecture, protocol, release, packaging, and hardware contracts.
- `assets/` stores icons/screenshots; `packaging/` contains desktop entry, systemd, and Windows preview packaging; `scripts/` contains maintenance helpers. Keep generated output in `build*/` and `dist/`.

## Build, Test, and Development Commands

Run commands from the repository root:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
cmake --build build --target all_qmllint
cmake --preset windows-local
cmake --build --preset windows-local
ctest --preset windows-local
cmake --preset linux-sanitizer
cmake --build --preset linux-sanitizer
ctest --preset linux-sanitizer
DESTDIR="$PWD/dist/linux-stage" cmake --install build --prefix /usr
.\packaging\windows\package.ps1 -BuildDir .\build-windows
```

These configure, build, test with failure output, run CI QML checks, run optional Linux sanitizer verification, stage Linux package inputs, and create a Windows preview ZIP from PowerShell. Without presets, use `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`. Windows contributors should copy `CMakeUserPresets.json.example` to the ignored `CMakeUserPresets.json`, configure `windows-local`, and make sure the Qt and MinGW `bin` directories are on `PATH` when running build or test commands directly.

Linux discovery builds only on Linux. Windows discovery builds only on Windows. The ASUS Aura HID backend builds on supported Linux and Windows configurations when hidapi is available; Windows uses a system hidapi package when found, otherwise the bundled HIDAPI Windows backend is used by default. On a Windows host, run Linux presets inside WSL with Qt 6.5+, hidapi, and libusb dev packages installed, working from the repository under `/mnt/<drive>/...`, e.g. `wsl -- bash -lc "cd /mnt/c/path/to/LumaCore && cmake --preset linux-debug"`. The native Windows presets configure into `build-windows/` while the Linux/WSL presets use `build/`, so the same checkout can hold both platform build trees at once without deleting either tree. (`.clangd` points at `build/`, so the Linux/WSL tree drives editor tooling.)

For a safe local session, run `./build/lumacore-daemon --allow-unprivileged --backend mock --socket /tmp/lumacore.sock` and `./build/lumacore --socket /tmp/lumacore.sock`; never use real hardware writes for routine UI development. Windows preview builds may expose read-only HID inventory and the guarded ASUS Aura HID backend, but real writes must stay dry-run-first and require owned validated hardware, config-table verification, dry-run off, and per-session confirmation.

## Coding Style & Naming Conventions

Use four-space indentation in C++, CMake, and QML. Follow Qt style: `PascalCase` types, `camelCase` functions/locals, `m_` members, and paired `.h`/`.cpp` files. Keep C++ in `lumacore`, prefer `QStringLiteral`, and use RAII or smart pointers. QML components use `PascalCase.qml`; properties and IDs use `camelCase`. Run `all_qmllint` before UI changes.

## Testing Guidelines

Tests are standalone C++ executables in `tests/CMakeLists.txt`. Name files `<Subject>Test.cpp` and CTest cases in `snake_case`. Add focused regressions for changed behavior, especially daemon framing, profile persistence and planning, settings and schedule behavior, daemon launching, permission gates, and hardware serialization. No coverage threshold is enforced; applicable Linux and Windows tests must pass.

## Commits, Pull Requests, and Safety

Recent commits use short release or imperative summaries such as `Release v0.9.0 daily-driver controls`, `Harden ASUS Aura HID write validation`, and `Fix CI: ...`. Keep each commit scoped.

Pull requests should explain behavior and safety impact, link issues, report build/test/QML-lint results, and include screenshots for UI changes. Preserve the GUI/daemon trust boundary and dry-run, allowlist, config-verification, and per-device confirmation gates. Treat protocol fields, profile formats and paths, IDs, AppController/QML invokables, backend ordering, ASUS packet bytes, Windows discovery behavior, and Windows preview behavior as compatibility-sensitive; consult `docs/architecture.md`, `docs/daemon/protocol.md`, `docs/refactor-parity.md`, `docs/release-verification.md`, `docs/hardware/asus-aura-hid.md`, and `docs/windows-preview.md`.
