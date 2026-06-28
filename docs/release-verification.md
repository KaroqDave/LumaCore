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

## Windows mock preview

From a Windows Release build:

```powershell
.\packaging\windows\package.ps1 -BuildDir .\build-windows
```

Extract the ZIP, start `lumacore.exe`, and confirm the bundled daemon starts with the mock backend. The Windows preview remains mock-only for v1.1; do not add hardware discovery, writes, services, signing, or installer requirements to this checklist.
