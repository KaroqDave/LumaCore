# LumaCore Windows Preview

This is an unsigned preview for 64-bit Windows 10 and Windows 11.

## Run

1. Extract the complete ZIP to a writable folder.
2. Double-click `lumacore.exe`.
3. Keep `lumacore-daemon.exe` beside it.

The GUI starts the bundled daemon without a console window and connects through a local endpoint restricted to the current Windows user. The companion daemon exits when the last connected LumaCore GUI closes.

## Current limitations

- Windows HID discovery is read-only and available when the build finds system hidapi or uses the bundled HIDAPI Windows backend.
- If hidapi is unavailable or no HID devices are reported, the auto backend falls back to mock devices.
- ASUS Aura HID writes are available only for the validated `0B05:19AF` controller through the `asus-aura-hid` backend after config-table verification, dry-run is disabled, and the device is confirmed for the current daemon session.
- Fresh Windows GUI settings and direct daemon sessions default to dry-run enabled; saved user choices are preserved.
- No Windows service, installer, automatic startup, elevation, code signing, Dynamic Lighting, or LampArray support.
- Profiles, settings, and Qt cache data stay beside the extracted executable under `data/`; no installer or registry-backed settings are used for the preview build.

## Diagnostics

Open Settings -> Windows diagnostics to confirm the detected Windows version, Qt runtime, active backend, daemon endpoint, bundled daemon presence, and profile storage path. With a hidapi-enabled build, the backend may show `windows-discovery` devices as read-only inventory. Use Export to save a redacted JSON report, or Copy Summary for a short support summary. The same export is also available from the Active Backend dialog.

If automatic startup fails, restore both executables from the ZIP. Advanced users can run `lumacore-daemon.exe --backend auto` manually, or use `--backend mock` for a simulated-device session, then start `lumacore.exe --no-auto-start-daemon`.
