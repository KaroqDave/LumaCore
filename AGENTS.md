# Repository Guidelines

## Project Structure & Module Organization

LumaCore is a C++23/Qt 6 application split between an unprivileged Qt Quick GUI and a hardware-facing daemon.

- `app/` contains startup, command-line options, and QML hosting.
- `core/` owns RGB models, profiles, effects, backend registration, and write policy.
- `daemon/`, `ipc/`, and `backends/` implement the daemon boundary and concrete backends.
- `hardware/linux/` contains Linux probes and hardware protocol serializers.
- `ui/` and `ui/qml/` contain controllers, models, and Qt Quick components.
- `tests/` contains CTest executables; `docs/` records architecture and protocol contracts.
- `assets/` stores icons and screenshots. Keep generated output in `build/`.

## Build, Test, and Development Commands

Run commands from the repository root:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
cmake --build build --target all_qmllint
```

These configure a Ninja debug build, compile it, run tests with failure output, and apply the QML checks enforced by CI. Without presets, use `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`. Windows contributors should configure `windows-local` as described in `README.md`.

For a safe local session, run the daemon with `--allow-unprivileged --backend mock`; never use real hardware writes for routine UI development.

## Coding Style & Naming Conventions

Use four-space indentation in C++, CMake, and QML. Follow the existing Qt style: `PascalCase` types, `camelCase` functions and locals, `m_` member prefixes, and paired `.h`/`.cpp` files. Keep C++ in the `lumacore` namespace, prefer `QStringLiteral`, and use RAII or smart pointers. QML components use `PascalCase.qml`; properties and IDs use `camelCase`. Run `all_qmllint` before submitting UI changes.

## Testing Guidelines

Tests are standalone C++ executables registered in `tests/CMakeLists.txt`. Name files `<Subject>Test.cpp` and CTest cases in `snake_case`. Add focused regression tests for changed behavior, especially daemon framing, profile persistence, permission gates, and hardware serialization. No numeric coverage threshold is enforced; all applicable Linux and Windows tests must pass.

## Commits, Pull Requests, and Safety

Recent commits use short, imperative summaries such as `Add Windows preview...`, `Fix CI: ...`, and `Update CMake configuration...`. Keep each commit scoped.

Pull requests should explain behavior and safety impact, link relevant issues, report build/test/QML-lint results, and include screenshots for visible UI changes. Preserve the GUI/daemon trust boundary and the dry-run, allowlist, config-verification, and per-device confirmation gates. Treat protocol fields, profile formats, IDs, and hardware packet bytes as compatibility-sensitive; consult `docs/architecture.md` and `docs/refactor-parity.md`.
