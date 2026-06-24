# Hardware Contribution Workflow

LumaCore accepts new hardware support only when the safety boundary is explicit, reviewable, and testable.
Discovery-only support and write-capable support are separate milestones. A device may be visible in inventory
long before it is allowed to change physical RGB state.

## Required Safety Model

Every hardware contribution must preserve these rules:

- The GUI remains unprivileged. Hardware access stays in `lumacore-daemon`.
- Discovery and probing are read-only unless the backend is explicitly documented as write-capable.
- Raw packet or register writes are not exposed through the daemon protocol.
- Real writes require backend capability checks, dry-run support, runtime permission checks, and the existing write gate.
- New write-capable hardware must be allowlisted by stable identity before it can be enabled.
- Hardware writes must be volatile lighting changes only. Firmware writes, EEPROM writes, persistent controller configuration, address scanning, and bus-wide probing are out of scope.
- Packet serializers must reject unsupported effects, invalid zone indices, invalid lengths, and unknown modes rather than guessing.
- The first safe physical test should be all-off or low-brightness static color after explicit user confirmation.

## Contribution Stages

### 1. Research-only device note

Use this stage for devices that are not implemented yet.

Required evidence:

- Vendor, product name, and stable identifiers such as USB VID/PID, HID usage page, interface number, or bus path shape.
- Operating-system inventory output with serial numbers and personal paths removed.
- Source provenance for protocol claims, such as owned-hardware captures, vendor documentation, or GPL-compatible source references.
- Clear statement of unknowns and unsafe assumptions.

Allowed code changes:

- Documentation only.
- Read-only parser tests using synthetic or sanitized sample data.

Not allowed:

- Enabling writes.
- Adding generic write paths.
- Adding broad VID/PID globs.

### 2. Read-only discovery

Use this stage when LumaCore can safely show the device in inventory.

Required evidence:

- Probe path is read-only.
- Device identity is stable enough for profiles and diagnostics.
- Duplicate physical interfaces are deduplicated.
- Permission-denied behavior is non-fatal and visible in diagnostics/activity logs.

Required tests:

- Probe parser or discovery normalization tests.
- Deduplication tests when the device exposes multiple interfaces.
- Permission/error handling tests.

Allowed capabilities:

- `DiscoveryRead`

Not allowed:

- `ZoneColorWrite`
- `ZoneEffectWrite`
- `allOff` hardware writes

### 3. Dry-run write preview

Use this stage before any physical RGB writes are accepted.

Required evidence:

- Packet or command structure is documented.
- Serializer output is deterministic and covered by tests.
- Preview text clearly states that the packet is not hardware-approved.
- Unsupported modes return explicit errors.

Required tests:

- Serializer tests for representative static/effect/all-off commands.
- Boundary tests for zone index, LED count, color, brightness, speed, report length, and unsupported effects.
- Tests proving preview packets cannot be submitted as approved hardware writes.

Allowed behavior:

- Dry-run previews.
- Activity-log summaries.

Not allowed:

- Opening a write-capable device handle for physical writes.
- Marking packets as hardware-approved.

### 4. Guarded write enablement

Use this stage only after owned-hardware validation.

Required evidence:

- Owned hardware used for testing is listed by stable identity.
- Probe confirms the exact controller family before write capability is exposed.
- Packet provenance is documented and GPL-compatible with LumaCore.
- The implemented writes are volatile and reversible.
- The contributor confirms no persistent configuration, firmware, EEPROM, or calibration writes are performed.

Required gates:

- Explicit allowlist entry for each write-capable identity.
- Backend capability bits only on verified devices.
- Dry-run must remain available.
- `WriteGate` confirmation must be required for real writes.
- Confirmation must be session-scoped and cleared on daemon restart/backend reinitialization.
- A clear reason must be shown when writes are unavailable.

Required tests:

- Allowlist tests.
- Config/probe verification tests.
- Serializer golden tests.
- Rejection tests for unsupported effects and invalid packet inputs.
- Write-gate tests proving unconfirmed writes are blocked.
- All-off tests if the backend exposes all-off.

## PR Checklist

Include this checklist in hardware PRs:

```md
## Hardware Validation

- [ ] This PR is documentation-only, read-only discovery, dry-run preview, or guarded write enablement.
- [ ] GUI remains unprivileged; hardware access stays in the daemon.
- [ ] No raw packet/register write method is exposed through IPC.
- [ ] No firmware, EEPROM, persistent configuration, calibration, address scanning, or bus-wide write is performed.
- [ ] Device identity and duplicate-interface behavior are documented.
- [ ] Protocol/source provenance is documented and license-compatible.
- [ ] Dry-run output is available for every write-capable operation.
- [ ] Real writes are allowlisted by exact stable identity.
- [ ] Real writes require per-session confirmation through `WriteGate`.
- [ ] Unsupported modes fail closed with explicit errors.
- [ ] Tests cover parser/probe behavior, serializer output, invalid inputs, permission failures, and write gates.
- [ ] Diagnostics/activity output contains enough information to debug failures without leaking personal paths or serials.

## Tested Hardware

- Vendor/product:
- Stable ID, such as VID:PID:
- Interface/usage details:
- Operating system and kernel:
- LumaCore backend:
- First physical test performed:
- Result:
- Known limitations:
```

## Review Rules

Reviewers should reject hardware write support when:

- The protocol source is unclear or license-incompatible.
- The implementation guesses unknown packet bytes without documenting the risk.
- A generic transport writer can be reached from the GUI or IPC layer.
- The backend enables writes for a family of devices instead of exact validated identities.
- Dry-run, confirmation, or permission gates are bypassed.
- Tests cover only successful writes and not rejection paths.

When in doubt, merge read-only discovery first and leave writes disabled.
