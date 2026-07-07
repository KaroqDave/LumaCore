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
cmake -S . -B build-linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux-release
ctest --test-dir build-linux-release --output-on-failure
cmake --build build-linux-release --target all_qmllint
rm -rf dist/linux-stage dist/LumaCore-Linux-x64 dist/LumaCore-Linux-x64.tar.gz
DESTDIR="$PWD/dist/linux-stage" cmake --install build-linux-release --prefix /usr
```

Confirm the staged tree contains `lumacore`, `lumacore-daemon`, the desktop entry, hicolor icons, and the configured systemd unit. Do not enable or start the service as part of verification.

Create the Linux release archive from the staged tree:

```sh
mkdir -p dist/LumaCore-Linux-x64
cp -a dist/linux-stage/usr dist/LumaCore-Linux-x64/
cp LICENSE dist/LumaCore-Linux-x64/
cp docs/linux-package.md dist/LumaCore-Linux-x64/README-Linux.md
tar -C dist -czf dist/LumaCore-Linux-x64.tar.gz LumaCore-Linux-x64
sha256sum dist/LumaCore-Linux-x64.tar.gz
```

Verify the packaged binaries report the release version:

```sh
./dist/LumaCore-Linux-x64/usr/bin/lumacore --version
./dist/LumaCore-Linux-x64/usr/bin/lumacore-daemon --version
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./dist/LumaCore-Linux-x64/usr/bin/lumacore --self-test
```

## Windows package

From a Windows shell with `CMakeUserPresets.json` configured for the local Qt and MinGW install:

```powershell
cmake --preset windows-local-release
cmake --build --preset windows-local-release
ctest --preset windows-local-release
.\packaging\windows\package.ps1 -BuildDir .\build-windows-release
```

Extract the ZIP and smoke the packaged runtime:

```powershell
Expand-Archive .\dist\LumaCore-Windows-x64.zip .\dist\windows-smoke -Force
Push-Location .\dist\windows-smoke\LumaCore-Windows-x64
.\lumacore.exe --version
.\lumacore-daemon.exe --version
$env:QT_QUICK_BACKEND = "software"
.\lumacore.exe --self-test
Pop-Location
```

Extract the ZIP, start `lumacore.exe`, and confirm the bundled daemon starts with the `auto` backend. In Settings -> Windows diagnostics, confirm the bundled daemon is present, the daemon endpoint is populated, dry-run is enabled on fresh settings, Export creates a JSON diagnostics report, and Copy Summary updates the status message. Confirm CMake reported a hidapi provider, either system or bundled. If HID devices are reported, confirm non-ASUS devices appear as read-only `windows-discovery` inventory. For an owned validated `0B05:19AF` ASUS Aura controller, first verify dry-run previews and confirmation prompts; only then disable dry-run and confirm one controlled static color write. If no devices are reported, confirm mock fallback still loads. Windows services, signing, and installer checks belong to later phases.

## GitHub release assets

Attach both release archives to the tag:

```sh
gh release upload vX.Y.Z dist/LumaCore-Windows-x64.zip dist/LumaCore-Linux-x64.tar.gz
gh release view vX.Y.Z --json assets
```

The release notes should name both downloads, mention that the Linux archive is a staged install tree, and include the Windows and Linux verification summary.
