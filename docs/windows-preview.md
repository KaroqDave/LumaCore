# LumaCore Windows Package

LumaCore's Windows packaging produces an unsigned portable package for 64-bit Windows 10 and Windows 11. No packaged releases are published yet; the ZIP is uploaded as a CI artifact by the Build workflow and can be built locally with `packaging\windows\package.ps1`.

## Get and Run

1. Get `LumaCore-Windows-x64.zip` from a recent Build workflow run's artifacts, or build it locally.
2. Extract the complete ZIP to a writable folder.
3. Double-click `lumacore.exe`.
4. Keep `lumacore-daemon.exe` beside it.

The GUI starts the bundled daemon without a console window and connects through a local endpoint restricted to the current Windows user. The companion daemon exits when the last connected LumaCore GUI closes.

## Safe Mock Session

For UI testing without hardware access, run the mock backend manually.

Terminal 1:

```powershell
.\lumacore-daemon.exe --backend mock
```

Use `--mock-scenario read-only`, `confirmation-required`, `failing-writes`, or `many-zones` to exercise specific UI and daemon paths without hardware. The default scenario is `asus-board`.

Terminal 2:

```powershell
.\lumacore.exe --no-auto-start-daemon
```

You can also pass a custom endpoint to both processes.

Terminal 1:

```powershell
.\lumacore-daemon.exe --backend mock --socket lumacore-test
```

Terminal 2:

```powershell
.\lumacore.exe --no-auto-start-daemon --socket lumacore-test
```

## Hardware Behavior

- Windows HID discovery is read-only and available when the build finds system hidapi or uses the bundled HIDAPI Windows backend.
- If hidapi is unavailable or no HID devices are reported, the `auto` backend falls back to mock devices.
- ASUS Aura HID writes are available only for the validated `0B05:19AF` controller through the `asus-aura-hid` backend after config-table verification, dry-run is disabled, and the device is confirmed for the current daemon session.
- Fresh Windows GUI settings and direct daemon sessions default to dry-run enabled; saved user choices are preserved.
- No Windows service, installer, automatic startup entry, elevation helper, code signing, Dynamic Lighting, or LampArray support is included yet.
- Profiles, settings, logs, and Qt cache data stay beside the extracted executable under `data/`; no installer or registry-backed settings are used for the portable package.

## Diagnostics

Open Settings -> Windows diagnostics to confirm the detected Windows version, Qt runtime, active backend, daemon endpoint, bundled daemon presence, GUI dry-run state, daemon-reported dry-run state, and profile storage path. The GUI backend is the `daemon` proxy; diagnostics also report the daemon-selected effective backend such as `auto`, `mock`, or `asus-aura-hid`. With a hidapi-enabled build, the backend may show `windows-discovery` devices as read-only inventory. Use Export to save a redacted JSON report, or Copy Summary for a short support summary. The same export is also available from the Active Backend dialog.

If automatic startup fails, restore both executables from the ZIP. Advanced users can run `lumacore-daemon.exe --backend auto` manually, or use `--backend mock` for a simulated-device session, then start `lumacore.exe --no-auto-start-daemon`.
