# CLAUDE.md

@AGENTS.md

AGENTS.md above is authoritative for structure, commands, style, tests, working style, hardware safety, and commits — don't restate it. This file only adds what is specific to this machine.

## This machine (Windows dev box)

- Default build/test here: `cmake --preset windows-local`, `cmake --build --preset windows-local`, `ctest --preset windows-local` → `build-windows/` (Qt 6.11.1 + MinGW, wired up in `CMakeUserPresets.json`).
- Linux-only code (`backends/linux/`, `hardware/linux/`, sanitizer runs) builds in WSL. From the repository root in PowerShell:
  `wsl --cd "$PWD" -- bash -lc "cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug"` → `build/`.
- `build-windows/` and `build/` are the two live trees (clangd reads `build-windows/`). All other `build-*` directories are one-off verification trees: never assume they are configured, never delete or "clean up" any build tree.
- Full verification for a cross-platform change: windows-local build + ctest, linux-debug build + ctest in WSL, and `all_qmllint` when QML changed. A Windows-only or Linux-only change needs its own platform plus a compile check that the other platform's shared code still builds.
- QML lint (Windows): `cmake --build build-windows --target all_qmllint`.
