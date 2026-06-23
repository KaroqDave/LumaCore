# ASUS Aura USB HID Notes

LumaCore supports an allowlisted ASUS Aura USB HID controller through the privileged daemon. The path is standard when the daemon runs with the default `--backend auto`, but real hardware writes remain gated by device allowlisting, config-table verification, dry-run state, approved packet builders, and per-session UI confirmation.

Broader ASUS or non-ASUS hardware support must follow the staged workflow in `docs/hardware/contributing-hardware.md`. Research notes, read-only discovery, dry-run previews, and guarded write enablement are separate milestones.

## Candidate

- Transport: USB HID via hidapi.
- Observed family: ASUS Aura LED Controller.
- Initial allowlist key: `VID:PID 0B05:19AF`.
- Backend id: `asus-aura-hid`.
- Current safe model: one motherboard-shaped controller with zones derived from the verified ASUS Aura config table where available, with a three-header fallback only when the config table is unavailable.
- Locally observed owned hardware: `0B05:19AF ASUSTek Computer, Inc. AURA LED Controller`.

## ASUS RGB Family Boundary

The research corpus treats ASUS RGB as several partially compatible controller families rather than one reusable protocol. LumaCore currently implements only the USB HID Aura LED Controller path above. Legacy Aura motherboard and DRAM control over SMBus/i2c, Aura Core laptop keyboards, ASUS System Control Interface / ACPI / WMI laptop paths, ASUS GPU-side lighting, Windows Dynamic Lighting / LampArray bridges, and newer receiver/dongle protocols are documentation and future-discovery subjects only.

Related ASUS Aura USB controller PIDs reported by OpenRGB and liquidctl research should be recorded as researched identities before they are made write-capable. The currently validated write target remains `0B05:19AF`; nearby researched devices such as `0B05:18F3` and `0B05:1939` are cataloged for read-only discovery only and must not be enabled for writes without owned-hardware captures, config-table tests, and explicit packet verification.

## Safety Boundary

- `linux-discovery` remains read-only and must not open or write HID devices.
- Real writes are enabled only through the guarded paths described below.
- The dry-run serializer still produces a LumaCore research preview report for logging and tests only.
- The preview serializer sets `hardwareWriteApproved=false`; the daemon-side device refuses to call the writer for that packet.
- Real writes use separate OpenRGB-referenced ASUS Aura USB packets and require a successful `EC B0`/`EC 30` config-table probe, dry-run off, and per-session confirmation.
- `WriteGate` returns dry-run previews but blocks real writes through `RequiresConfirmation` unless the daemon has a session confirmation token for the device.
- GUI dry-run requests ask the daemon for ASUS-specific preview text before the GUI write gate stops the operation, so packet summaries still describe the real daemon-side ASUS device.

## Current Dry-Run Behavior

- Discovery is limited to the allowlisted `0B05:19AF` controller and collapses duplicate hidapi interfaces into one physical device entry.
- Candidate HID interfaces are probed with an ASUS Aura `EC B0` config-table request; interfaces that return `EC 30` are preferred over heuristic-only matches.
- The device id is stable by VID/PID rather than by hidraw path.
- Legacy research previews accept zone indices `0..2` and include header number, LED count, brightness-scaled RGB, first report bytes, all-off status, and `hardwareWriteApproved=false`.
- The preview report stores the selected header index for logging and tests, but the opcode remains a LumaCore research marker, not a validated ASUS write command.
- Static black or brightness `0` is labeled as an all-off preview; it is still not a hardware-approved all-off command.

## Guarded Real-Write Behavior

- Real writes are limited to the allowlisted `0B05:19AF` ASUS Aura LED Controller.
- The GUI remains unprivileged; only `lumacore-daemon` opens the HID path.
- The daemon defaults to `--backend auto`, which aggregates verified ASUS hardware with read-only Linux discovery inventory. `--backend asus-aura-hid` remains available as an explicit override.
- Discovery must verify an `EC 30` config-table response from the selected HID path. If no interface responds, the controller remains visible but write capability stays disabled and the activity/device details explain the probe failure.
- Dry-run must be off and the UI must confirm writes for the current daemon session.
- Confirmation is held in memory by the daemon and is cleared when the daemon restarts or the backend is reinitialized.
- Approved packets use 65-byte reports based on GPL-compatible OpenRGB protocol references. The sequence starts with the OpenRGB mainboard `EC52 53 00 01` setup packet.
- Fixed RGB headers are targeted from the parsed config table with `EC35` static mode plus `EC36` color data at the computed RGB-header LED offset, and are advertised as static-only.
- Addressable headers are targeted from the parsed config table with `EC35` direct mode plus chunked `EC40` direct color packets, with the apply bit set only on the final chunk.
- Native effects on individual fixed RGB headers are blocked because those headers share one channel-wide `EC35` effect channel. Addressable headers advertise native color cycle and rainbow only; color-bearing native effects such as breathing remain blocked until the EC36 addressable color mapping is captured and tested.
- Color cycle uses native ASUS spectrum-cycle mode `0x04`.
- Rainbow uses native ASUS mode `0x05`.
- Native mode brightness is accepted only as `0` (off) or `100`; intermediate values are rejected until a hardware brightness field is verified.
- LumaCore stores effect speed in the UI/model and logs it in previews, but no ASUS speed payload byte is encoded until a verified field is documented.
- The All Off action sends one explicit `EC35` off-mode packet per parsed config channel and requires session confirmation.
- Successful and failed real HID writes are recorded in the activity log with interface/path, report count, byte count, and protocol summary or transport error.

## Licensing Boundary

LumaCore is licensed under GPL-2.0-or-later. The real-write protocol path is based on GPL-compatible OpenRGB references: [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB), the `0B05:19AF` detector mapping in [AsusAuraUSBControllerDetect.cpp](https://github.com/CalcProgrammer1/OpenRGB/blob/9f82afa4/Controllers/AsusAuraUSBController/AsusAuraUSBControllerDetect.cpp), the public [ASUS Aura USB protocol notes](https://openrgb-wiki.readthedocs.io/en/latest/asus/ASUS-Aura-USB/), and the X570 protocol write-up by inlart. Protocol details should still be independently validated with owned-hardware captures, vendor documentation, or clearly documented GPL-compatible sources before broadening beyond volatile color/effect writes on allowlisted controllers.

## Protocol Research Required Before Broader Hardware Writes

Before broadening the write path beyond the current allowlisted static/direct/native-effect implementation:

1. Capture HID descriptors and report lengths for the owned ASUS Aura controller.
2. Capture vendor software traffic on owned hardware, or use permissive/vendor documentation.
3. Document report id, opcode, color payload layout, commit behavior, and unknown bytes.
4. Add serializer tests using independently derived packet examples.
5. Validate per-header channel mapping and report-ID behavior for this exact motherboard.
6. Validate native effect speed/direction fields before encoding hardware speed controls.
7. Test only volatile all-off or low-brightness static color after explicit user confirmation.
8. Keep discovery/read-only interrogation separate from write enablement for every new ASUS device family.

## Out Of Scope

- SMBus/i2c writes.
- Address scanning.
- Persistent hardware configuration.
- Firmware or EEPROM writes.
- Animated direct-mode streaming.
- Generic HID writes for non-allowlisted devices.
