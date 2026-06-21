# Repository Guidelines

## Project Structure & Module Organization

LumaCore is a C++23/Qt 6 RGB controller split between an unprivileged Qt Quick GUI and hardware-facing daemon.

- `app/` contains startup, command-line options, and QML hosting.
- `core/` owns RGB models, profiles, schedules, effects, backend registration, and write policy.
- `daemon/` and `ipc/` implement the daemon process and local protocol boundary.
- `backends/` contains `mock`, `auto`, `daemon`, Linux discovery, and ASUS Aura HID backends.
- `ui/` and `ui/qml/` contain controllers, models, and Qt Quick components.
- `hardware/linux/` contains read-only probes and protocol serializers.
- `tests/` contains CTest executables; `docs/` records architecture/protocol contracts.
- `assets/` stores icons/screenshots; `packaging/` contains systemd and Windows preview packaging. Keep generated output in `build*/` and `dist/`.

## Build, Test, and Development Commands

Run commands from the repository root:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
cmake --build build --target all_qmllint
./packaging/windows/package.ps1 -BuildDir ./build
```

These configure, build, test with failure output, run CI QML checks, and create a Windows preview ZIP from PowerShell. Without presets, use `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`. Windows contributors should configure `windows-local` per `README.md`.

For a safe local session, run the daemon with `--allow-unprivileged --backend mock`; never use real hardware writes for routine UI development.

## Coding Style & Naming Conventions

Use four-space indentation in C++, CMake, and QML. Follow Qt style: `PascalCase` types, `camelCase` functions/locals, `m_` members, and paired `.h`/`.cpp` files. Keep C++ in `lumacore`, prefer `QStringLiteral`, and use RAII or smart pointers. QML components use `PascalCase.qml`; properties and IDs use `camelCase`. Run `all_qmllint` before UI changes.

## Testing Guidelines

Tests are standalone C++ executables in `tests/CMakeLists.txt`. Name files `<Subject>Test.cpp` and CTest cases in `snake_case`. Add focused regressions for changed behavior, especially daemon framing, profile persistence, permission gates, and hardware serialization. No coverage threshold is enforced; applicable Linux and Windows tests must pass.

## Commits, Pull Requests, and Safety

Recent commits use short release or imperative summaries such as `Release v0.9.0 daily-driver controls`, `Harden ASUS Aura HID write validation`, and `Fix CI: ...`. Keep each commit scoped.

Pull requests should explain behavior and safety impact, link issues, report build/test/QML-lint results, and include screenshots for UI changes. Preserve the GUI/daemon trust boundary and dry-run, allowlist, config-verification, and per-device confirmation gates. Treat protocol fields, profile formats, IDs, and hardware packet bytes as compatibility-sensitive; consult `docs/architecture.md`, `docs/daemon/protocol.md`, `docs/hardware/asus-aura-hid.md`, and `docs/windows-preview.md`.
