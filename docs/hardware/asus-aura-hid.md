# ASUS Aura USB HID Notes

LumaCore supports an allowlisted ASUS Aura USB HID controller through the privileged daemon. The path is standard when the daemon runs with the default `--backend auto`, but real hardware writes remain gated by device allowlisting, config-table verification, dry-run state, approved packet builders, and per-session UI confirmation.

Broader ASUS or non-ASUS hardware support must follow the staged workflow in `docs/hardware/contributing-hardware.md`. Research notes, read-only discovery, dry-run previews, and guarded write enablement are separate milestones.

## Candidate

- Transport: USB HID via hidapi.
- Observed family: ASUS Aura LED Controller.
- Initial allowlist key: `VID:PID 0B05:19AF`.
- Backend id: `asus-aura-hid`.
- Current safe model: one motherboard-shaped controller with zones derived from the verified ASUS Aura config table where available, with addressable headers defaulting to the LumaScope-validated 120-LED `EC40` capacity and a three-header fallback only when the config table is unavailable.
- Locally observed owned hardware: `0B05:19AF ASUSTek Computer, Inc. AURA LED Controller`.

## ASUS RGB Family Boundary

The research corpus treats ASUS RGB as several partially compatible controller families rather than one reusable protocol. LumaCore currently implements only the USB HID Aura LED Controller path above. Legacy Aura motherboard and DRAM control over SMBus/i2c, Aura Core laptop keyboards, ASUS System Control Interface / ACPI / WMI laptop paths, ASUS GPU-side lighting, Windows Dynamic Lighting / LampArray bridges, and newer receiver/dongle protocols are documentation and future-discovery subjects only.

Related ASUS Aura USB controller PIDs reported by OpenRGB and liquidctl research should be recorded as researched identities before they are made write-capable. The currently validated write target remains `0B05:19AF`; nearby researched controllers (`0B05:18F3`, `0B05:1939`, `0B05:1867`, `0B05:1872`, `0B05:18A3`, `0B05:18A5`, `0B05:1AA6`, and the `0B05:1889` ROG Aura Terminal) are cataloged in `docs/hardware/discovery-catalog.md` for read-only discovery only and must not be enabled for writes without owned-hardware captures, config-table tests, and explicit packet verification.

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
- The daemon defaults to `--backend auto`, which aggregates verified ASUS hardware with read-only platform discovery inventory. `--backend asus-aura-hid` remains available as an explicit override on supported Linux and Windows builds.
- Discovery must verify an `EC 30` config-table response from the selected HID path. If no interface responds, the controller remains visible but write capability stays disabled and the activity/device details explain the probe failure.
- Dry-run must be off and the UI must confirm writes for the current daemon session.
- Confirmation is held in memory by the daemon and is cleared when the daemon restarts or the backend is reinitialized.
- Approved packets use 65-byte reports based on GPL-compatible OpenRGB protocol references. The sequence starts with the OpenRGB mainboard `EC52 53 00 01` setup packet.
- Fixed mainboard zones are read-only by policy: they are exposed from the parsed config table for discovery and config verification, advertise no effect support, and are never written — not by static applies, not by profiles or global applies, and not by All Off. The `EC35` static mode plus `EC36` fixed-color serializer remains research reference code that the device never sends.
- Addressable static colors are targeted from the parsed config table with `EC35` direct mode plus chunked `EC40` direct color packets, with the apply bit set only on the final chunk.
- Addressable animated effects are host-streamed over the same LumaScope-validated `EC40` direct-color path. Applying an effect primes direct mode, paints the first frame, then the daemon streams per-LED frames through `applyZoneFrame` while normal dry-run and confirmation gates remain in force.
- Addressable headers advertise rainbow, breathing, color-cycle, wave, marquee, and strobe with speed and brightness support because all of them are host-rendered before serialization; new host-streamed effect types stay addressable-only and need no packet changes. Fixed mainboard zones advertise nothing: their native channel mapping is shared, their Armoury Crate capture reassembled through a separate channel that is not enabled for LumaCore writes, and policy keeps them read-only.
- LumaCore stores effect speed in the UI/model, maps it to a host-side animation period, and does not encode a speed payload byte. Owned-hardware capture (see the speed finding below) confirms this controller has no wire speed field under Armoury Crate — the vendor renders effect speed as a host-side animation phase-step, not a device command.
- The older `EC35`/`EC36` native effect serializer remains OpenRGB-referenced research code for later validation, but it is no longer the advertised addressable animated route.
- The All Off action sends one explicit `EC35` off-mode packet per parsed addressable config channel and requires session confirmation. The read-only fixed mainboard channel is skipped and keeps whatever state the firmware or other software last set.
- Successful and failed real HID writes are recorded in the activity log with interface/path, report count, byte count, and protocol summary or transport error.

## Owned-Hardware Capture Validation (LumaScope)

The `EC40` addressable direct-color path was validated against owned hardware on `0B05:19AF`
(AURA LED Controller) with [LumaScope](https://github.com/KaroqDave/LumaScope), a companion
reverse-engineering harness. The Armoury Crate stack passes the color buffer by reference through a
kernel driver, so the bytes are invisible to in-process hooks; they were captured at the USB bus
level with USBPcap. A controlled single-color capture (board set to pure red) confirmed the wire
format that LumaCore's `appendDirectColorReports` already emits:

- report `0xEC`, command `0x40` (direct color);
- byte 2 = `channel | 0x80` with the apply bit set only on the final chunk;
- byte 3 = LED offset and byte 4 = LED count, both **in LEDs** (payload = `count * 3` bytes), 20 LEDs/packet;
- payload at byte 5 in **RGB order** — pure red on the wire is `ff 00 00`, confirmed by the
  controlled single-color capture (an earlier *passive* red→green→blue capture had wrongly suggested
  GRB; the single-variable capture corrected it).

This satisfies items 2–3 of "Protocol Research Required Before Broader Hardware Writes" for the
addressable direct-color path. The full capture, decode, and write-up live in LumaScope's
`docs/asus-aura-pid19af-protocol.md`, and `tests/AsusAuraHidProtocolTest.cpp` now asserts
`buildAsusAuraStaticColorWrite` produces these exact bytes.

### Armoury Crate uses only `EC40` — `EC35`/`EC36` are not Aura-observable (finding)

Two controlled capture sets — Static, Breathing, Color-cycle and Rainbow, on **both** an addressable
and a fixed RGB zone (seven captures total) — were taken to locate the assumed `EC35` mode-set and
`EC36` effect-color commands. **They do not appear.** Across every capture the *only* ASUS color
command class is `EC40`; a raw scan for `EC35`/`EC36`/`EC30`/`ECB0`/`EC52` returns zero matches.
Armoury Crate renders every mode in software and streams direct-color frames: Breathing ramps the
per-LED value `0→255→0` on a 42-step gamma curve, Color-cycle/Rainbow stream hue changes frame by
frame (~4000 frames/window), and even the *fixed* zone reassembles to the same `EC40` channels. RGB
byte order was re-confirmed (red ramps the R byte, green the G byte).

Consequence and limit: this controller, under Armoury Crate, is a **direct-streaming** implementation.
Capturing Armoury Crate — no matter the effect or zone — will never exercise an `EC35`/`EC36` path, so
it can neither confirm nor deny LumaCore's config-table / static-mode / native-effect model. That model
derives from **OpenRGB**, which uses `EC35`/`EC36` and the `ECB0`/`EC30` probe for **hardware-persistent**
effects (lighting that survives after the app closes) — a capability Armoury Crate does not use.

Two takeaways for this backend:

1. **The `EC35`/`EC36` fixed-header and native-effect paths remain OpenRGB-referenced, not
   owned-capture-validated, and cannot become so via an Armoury Crate capture.** Validating them on
   owned hardware requires a *different* reference: a capture of OpenRGB driving this board, or a
   guarded dry-run→write test from LumaCore itself (daemon-side, Armoury Crate closed). Until then the
   existing gates on those paths are correct and should stay.
2. **The entire Armoury Crate feature set — all effects, fixed and addressable — is reproducible over
   the already-validated `EC40` direct path alone** (`appendDirectColorReports`), host-streamed.
   LumaCore now uses that certain path for addressable rainbow, breathing, and color-cycle effects
   without depending on the unvalidated `EC35`/`EC36` native effect commands.

### Direct-channel numbering (addressable `0/1/2`, fixed mainboard `0x04`) — confirmed

The capture addresses the three addressable headers as direct channels `0/1/2` and the small onboard
fixed zone as direct channel `0x04` (`EC 40 84 00 02 …`). This matches OpenRGB's mainboard controller
exactly: `AsusAuraMainboardController` hardcodes the fixed mainboard device to `direct_channel = 0x04`
and enumerates addressable headers as `direct_channel = i` (`0,1,2,…`). The channel index is a fixed
convention, not a per-entry field — the `EC 30` config table is a flat 60-byte block carrying only
counts at `0x02` (addressable headers), `0x1B` (mainboard LEDs), and `0x1D` (RGB headers).
`parseAsusAuraConfigTableResponse` already encodes this: the fixed channel is built with
`directChannel = 0x04` and each addressable channel with `directChannel = index`, so the parsed channel
map reproduces the captured numbering. No change is required to the channel mapping.

One transport nuance remains a future-validation item. Armoury Crate drives the fixed zone over the
validated `EC40` direct path (channel `0x04`), whereas LumaCore routes the fixed zone through the
`EC35`/`EC36` 16-bit-mask path and `buildAsusAuraDirectFrameWrite` rejects non-addressable targets.
Both are legitimate; the fixed path stays on the gated OpenRGB-referenced route until an owned-hardware
capture validates a fixed-zone `EC40` write for more than the single 2-LED zone observed here.

### Effect speed has no wire field — it is a host animation rate (finding)

The rainbow was captured at three speed-slider positions and the timing measured. Speed is **not**
a device parameter: across slider min→max the stream frame rate stayed ~180–204 fps (essentially
constant) while the per-frame hue step scaled ~9× and the full-cycle period moved from **16.5 s
(slowest) to 1.6 s (fastest)**. Armoury Crate holds a fixed refresh and advances the animation phase
further each frame — i.e. effect speed is a host-side phase increment, not a byte on the wire.

This resolves the "no verified speed field" note above: for this controller there is nothing to
encode on the streamed path. LumaCore maps the UI speed slider to a host animation period of roughly
**1.6 s (fast) to 16.5 s (slow)**, matching the measured vendor range. A native firmware speed field
could still exist behind the OpenRGB `EC35` effect path, but that is unconfirmed here and shares the
same validation route as the rest of that path.

## Licensing Boundary

LumaCore is licensed under GPL-2.0-or-later. The real-write protocol path is based on GPL-compatible OpenRGB references: [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB), the `0B05:19AF` detector mapping in [AsusAuraUSBControllerDetect.cpp](https://github.com/CalcProgrammer1/OpenRGB/blob/9f82afa4/Controllers/AsusAuraUSBController/AsusAuraUSBControllerDetect.cpp), the public [ASUS Aura USB protocol notes](https://openrgb-wiki.readthedocs.io/en/latest/asus/ASUS-Aura-USB/), and the X570 protocol write-up by inlart. Protocol details should still be independently validated with owned-hardware captures, vendor documentation, or clearly documented GPL-compatible sources before broadening beyond volatile color/effect writes on allowlisted controllers.

## Protocol Research Required Before Broader Hardware Writes

Before broadening the write path beyond the current allowlisted static/direct/native-effect implementation:

1. Capture HID descriptors and report lengths for the owned ASUS Aura controller.
2. Capture vendor software traffic on owned hardware, or use permissive/vendor documentation.
3. Document report id, opcode, color payload layout, commit behavior, and unknown bytes.
4. Add serializer tests using independently derived packet examples.
5. Validate per-header channel mapping and report-ID behavior for this exact motherboard.
6. Validate native effect mode/color/speed/direction fields before advertising hardware-persistent native effects.
7. Test only volatile all-off or low-brightness static color after explicit user confirmation.
8. Keep discovery/read-only interrogation separate from write enablement for every new ASUS device family.

## Out Of Scope

- SMBus/i2c writes.
- Address scanning.
- Persistent hardware configuration.
- Firmware or EEPROM writes.
- Native hardware-persistent effects beyond the current guarded OpenRGB-referenced research path.
- Generic HID writes for non-allowlisted devices.
