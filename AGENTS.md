# Repository Guidelines

## Project Structure & Module Organization

LumaCore is a C++23/Qt 6 application split between an unprivileged Qt Quick GUI and a hardware-facing daemon.

- `app/` contains startup, command-line options, and QML hosting.
- `core/` owns RGB models, profiles, effects, backend registration, and write policy.
- `daemon/`, `ipc/`, and `backends/` implement the daemon boundary and concrete backends.
- `hardware/linux/` contains Linux probes and hardware protocol serializers.
- `ui/` and `ui/qml/` contain controllers, models, and Qt Quick components.
- `tests/` contains CTest executables; `docs/` records architecture and protocol contracts.
- `assets/` stores icons and screenshots; generated build output belongs in `build/`.

## Build, Test, and Development Commands

From the repository root:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
cmake --build build --target all_qmllint
```

The first command configures Ninja and exports `build/compile_commands.json`; the next commands build, run all tests with failure output, and lint QML. Without presets, use `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`.

For safe local development, run the daemon with the mock backend:

```sh
./build/lumacore-daemon --allow-unprivileged --backend mock --socket /tmp/lumacore.sock
./build/lumacore --socket /tmp/lumacore.sock
```

## Coding Style & Naming Conventions

Use four-space indentation in C++, CMake, and QML. Follow existing Qt-oriented C++ style: `PascalCase` types, `camelCase` functions and locals, `m_` member prefixes, and paired `.h`/`.cpp` files. Keep code in the `lumacore` namespace, prefer `QStringLiteral`, mark useful return values `[[nodiscard]]`, and use RAII/smart pointers. QML components use `PascalCase.qml`; properties and IDs use `camelCase`. Run `all_qmllint` before submitting UI changes.

## Testing Guidelines

Tests are standalone C++ executables registered with CTest in `tests/CMakeLists.txt`. Name files `<Subject>Test.cpp` and CTest cases in `snake_case`. Add focused regression tests for changed behavior, especially daemon framing, profile persistence, permission gates, and hardware serialization. No numeric coverage threshold is enforced; all configured tests must pass on applicable Linux and Windows builds.

## Safety, Commits & Pull Requests

Preserve the GUI/daemon trust boundary and the dry-run, allowlist, config-verification, and per-device confirmation gates. Treat protocol fields, profile formats, backend/device IDs, and hardware packet bytes as compatibility-sensitive; consult `docs/architecture.md` and `docs/refactor-parity.md`.

Recent commits use short, imperative summaries such as `Add Windows preview...` or `Update CMake configuration...`. Keep commits scoped. Pull requests should explain behavior and safety impact, link relevant issues, list build/test/QML-lint results, and include screenshots for visible UI changes.
