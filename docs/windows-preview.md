# LumaCore Windows Preview

This is an unsigned preview for 64-bit Windows 10 and Windows 11.

## Run

1. Extract the complete ZIP to a writable folder.
2. Double-click `lumacore.exe`.
3. Keep `lumacore-daemon.exe` beside it.

The GUI starts the bundled daemon without a console window and connects through a local endpoint restricted to the current Windows user. The companion daemon exits when the last connected LumaCore GUI closes.

## Current limitations

- Mock devices only.
- No Windows hardware discovery or physical RGB writes.
- No Windows service, installer, automatic startup, elevation, code signing, Dynamic Lighting, or LampArray support.
- Profiles, settings, and Qt cache data stay beside the extracted executable under `data/`; no installer or registry-backed settings are used for the preview build.

If automatic startup fails, restore both executables from the ZIP. Advanced users can run `lumacore-daemon.exe --backend mock` manually, then start `lumacore.exe --no-auto-start-daemon`.
