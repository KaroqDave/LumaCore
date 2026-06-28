# Release Verification

Use this checklist before tagging a focused release.

For behavior-preserving refactors, pair these commands with the golden checks in
`docs/refactor-parity.md` and call out any intentionally deferred migration-only work in the PR.

## Linux debug verification

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
cmake --build build --target all_qmllint
```

## Linux sanitizer verification

Run this when the local Qt and compiler setup supports the sanitizer preset:

```sh
cmake --preset linux-sanitizer
cmake --build --preset linux-sanitizer
ctest --preset linux-sanitizer
```

## Linux install staging

```sh
DESTDIR="$PWD/dist/linux-stage" cmake --install build --prefix /usr
```

Confirm the staged tree contains `lumacore`, `lumacore-daemon`, the desktop entry, hicolor icons, and the configured systemd unit. Do not enable or start the service as part of verification.

## Windows preview

From a Windows shell with `CMakeUserPresets.json` configured for the local Qt and MinGW install:

```powershell
cmake --preset windows-local-release
cmake --build --preset windows-local-release
ctest --preset windows-local-release
.\packaging\windows\package.ps1 -BuildDir .\build-windows-release
```

Extract the ZIP, start `lumacore.exe`, and confirm the bundled daemon starts with the `auto` backend. In Settings -> Windows diagnostics, confirm the bundled daemon is present, the daemon endpoint is populated, Export creates a JSON diagnostics report, and Copy Summary updates the status message. If the build includes hidapi and reports HID devices, confirm they appear as read-only `windows-discovery` inventory. If hidapi is unavailable or no devices are reported, confirm mock fallback still loads. Windows physical RGB writes, services, signing, and installer checks belong to later phases.
