# Repository Guidelines

## Project Structure & Module Organization

LumaCore is a C++23/Qt 6.5+ RGB controller split between an unprivileged Qt Quick GUI and hardware-facing daemon.

- `app/` contains startup, command-line options, daemon launch/tray helpers, scheduling, and QML hosting.
- `core/` owns RGB models, profile storage/planning, schedules, effects, activity logging, backend registration, and write policy.
- `daemon/` and `ipc/` implement the daemon process, protocol v1 methods, local socket client/server, and shared frame codec.
- `backends/` contains `mock`, `auto`, `daemon`, Linux/Windows discovery, and ASUS Aura HID backends.
- `ui/` and `ui/qml/` contain QML-facing controllers, models, private preference stores, and Qt Quick components.
- `hardware/common/` contains cross-platform discovery catalog helpers.
- `hardware/asus/` contains the shared ASUS Aura HID protocol serializers (`lumacore::hardware::asus`).
- `hardware/linux/` contains Linux read-only probes and HID writer glue.
- `hardware/windows/` contains Windows HID discovery probes and HID writer glue.
- `tests/` contains CTest executables; `docs/` records architecture, protocol, release, packaging, and hardware contracts.
- `assets/` stores icons/screenshots; `packaging/` contains desktop entry, systemd, and Windows package tooling; `scripts/` contains maintenance helpers. Keep generated output in `build*/` and `dist/`.

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

These configure, build, test with failure output, run CI QML checks, run optional Linux sanitizer verification, stage Linux package inputs, and create a Windows package ZIP from PowerShell. Without presets, use `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`. Windows contributors should copy `CMakeUserPresets.json.example` to the ignored `CMakeUserPresets.json`, configure `windows-local`, and make sure the Qt and MinGW `bin` directories are on `PATH` when running build or test commands directly.

Linux discovery builds only on Linux. Windows discovery builds only on Windows. The ASUS Aura HID backend builds on supported Linux and Windows configurations when hidapi is available; Windows uses a system hidapi package when found, otherwise the bundled HIDAPI Windows backend is used by default. On a Windows host, run Linux presets inside WSL with Qt 6.5+, hidapi, and libusb dev packages installed, working from the repository under `/mnt/<drive>/...`, e.g. `wsl -- bash -lc "cd /mnt/c/path/to/LumaCore && cmake --preset linux-debug"`. The native Windows presets configure into `build-windows/` while the Linux/WSL presets use `build/`, so the same checkout can hold both platform build trees at once without deleting either tree. (`.clangd` points at `build-windows/`, so a configured native Windows build drives editor tooling; WSL editing needs a local `--compile-commands-dir=build` clangd override.)

For a safe local session, run `./build/lumacore-daemon --allow-unprivileged --backend mock --socket /tmp/lumacore.sock` and `./build/lumacore --socket /tmp/lumacore.sock`; never use real hardware writes for routine UI development. Windows builds may expose read-only HID inventory and the guarded ASUS Aura HID backend, but real writes must stay dry-run-first and require owned validated hardware, config-table verification, dry-run off, and per-session confirmation.

## Hardware Safety

Development sessions always use the mock backend with an explicit socket; never drive real hardware writes as part of routine work. Real-write validation happens only on owned hardware following the checklist in `docs/hardware/asus-aura-hid.md`.

- Never weaken, bypass, or "simplify away" a write gate: the dry-run default, the device allowlist (`0B05:19AF`), config-table verification, and per-session confirmation. A change that touches these paths must say so explicitly in its summary and extend the gating tests.
- Packet bytes in `hardware/asus/` are validated against real USB captures (LumaScope). Never adjust packet values from reasoning or vendor documentation alone — they change only with new capture evidence recorded in `docs/hardware/asus-aura-hid.md`.
- New hardware support follows the staged workflow in `docs/hardware/contributing-hardware.md`: research notes, then read-only discovery, then dry-run previews, then guarded write enablement — in that order, never skipping ahead.

## Coding Style & Naming Conventions

Use four-space indentation in C++, CMake, and QML. Follow Qt style: `PascalCase` types, `camelCase` functions/locals, `m_` members, and paired `.h`/`.cpp` files. Keep C++ in `lumacore`, prefer `QStringLiteral`, and use RAII or smart pointers. QML components use `PascalCase.qml`; properties and IDs use `camelCase`. Run `all_qmllint` before UI changes.

## Testing Guidelines

Tests are standalone C++ executables in `tests/CMakeLists.txt`. Name files `<Subject>Test.cpp` and CTest cases in `snake_case`. Add focused regressions for changed behavior, especially daemon framing, profile persistence and planning, settings and schedule behavior, daemon launching, permission gates, and hardware serialization. No coverage threshold is enforced; applicable Linux and Windows tests must pass.

## Working Style

- Act with stated assumptions instead of stalling. Ask before implementing only when the answer changes direction: compatibility-sensitive surfaces, safety gates, or genuine scope ambiguity. Otherwise pick the most reasonable interpretation, name it in your summary, and proceed.
- Prove behavior, don't assert it. Bug fixes start from a failing test where the code is unit-testable — and most of this repo is (frame codec, profile planner, write gates, option parsing, serializers, schedule logic). For UI or timing behavior that isn't, run a mock-daemon session or the `--self-test` QML smoke path and report what you observed. Always say which form of verification you performed.
- Keep diffs minimal and surgical: no drive-by refactors, no speculative abstraction or configurability, no error handling for impossible states. Match the idiom already in the file being edited. Mention unrelated dead code; don't remove it. Every changed line should trace to the request.
- Before finishing, apply the simplicity check: if a senior engineer would call the diff overbuilt, rewrite it smaller.
- Changes to compatibility-sensitive surfaces (listed under Pull Requests below) require reading the matching doc first and stating the compatibility impact — or "none, additive" — in the summary. An old GUI against a new daemon, and old profiles against new code, must keep working.

## Commits, Pull Requests, and Safety

Use Conventional Commit prefixes — `feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:` — with a concise, lowercase, scoped subject that names the command, backend, or protocol area affected (e.g. `feat: add host effect-frame streaming to the daemon protocol`, `fix: preserve chunked frame codec ordering`). Keep each commit scoped to one change.

Write a body that explains the *why* and the *what*, not a restated diff: the problem being solved, the reasoning, and any hardware/protocol findings or assumptions. Prose paragraphs or a `Changelog:` bullet list both work. Close every non-trivial commit with a verification section listing the exact commands run and their results, for example:

```
Verification:
- ctest --preset linux-debug (all passing)
- cmake --build build --target all_qmllint
- ./build/lumacore-daemon --allow-unprivileged --backend mock ... smoke run
```

The verification section must list only commands that were actually run, with their real results. If a platform leg was skipped (for example, no WSL run for a Windows-only change), write that down instead of implying full coverage. Never add AI attribution — no co-author trailers or generated-by footers — to commits or pull requests.

Release commits may keep a `Release vX.Y.Z <summary>` subject.

Pull requests should explain behavior and safety impact, link issues, report build/test/QML-lint results, and include screenshots for UI changes. Preserve the GUI/daemon trust boundary and dry-run, allowlist, config-verification, and per-device confirmation gates. Treat protocol fields, profile formats and paths, IDs, AppController/QML invokables, backend ordering, ASUS packet bytes, Windows discovery behavior, and Windows package behavior as compatibility-sensitive; consult `docs/architecture.md`, `docs/daemon/protocol.md`, `docs/refactor-parity.md`, `docs/release-verification.md`, `docs/hardware/asus-aura-hid.md`, and `docs/windows-preview.md`.
