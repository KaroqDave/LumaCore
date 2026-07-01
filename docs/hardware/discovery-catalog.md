# Read-Only Discovery Catalog

The discovery catalog records hardware identities that LumaCore can classify during inventory without granting write capability. It is intentionally separate from backend write allowlists.

## Stages

- `guarded-write-backend`: an exact identity has a separate write-capable backend, but `linux-discovery` still reports it as read-only inventory.
- `research-only`: the identity is useful for inventory and future research, but no dry-run or real-write behavior is approved.
- `heuristic`: provider text looks RGB-related, but the identity has not been cataloged.
- `unclassified`: ordinary read-only inventory with no RGB-specific signal.

## Current Catalog

| VID:PID | Family | Stage | Write backend |
| --- | --- | --- | --- |
| `0B05:19AF` | ASUS Aura USB HID | `guarded-write-backend` | `asus-aura-hid`, after config verification and confirmation |
| `0B05:18F3` | ASUS Aura USB HID | `research-only` | none |
| `0B05:1939` | ASUS Aura USB HID | `research-only` | none |
| `0B05:1867` | ASUS Aura USB HID | `research-only` | none |
| `0B05:1872` | ASUS Aura USB HID | `research-only` | none |
| `0B05:18A3` | ASUS Aura USB HID | `research-only` | none |
| `0B05:18A5` | ASUS Aura USB HID | `research-only` | none |
| `0B05:1AA6` | ASUS Aura USB HID | `research-only` | none |
| `0B05:1889` | ASUS Aura USB HID | `research-only` | none |

The `0B05:1867`, `0B05:1872`, `0B05:18A3`, `0B05:18A5`, and `0B05:1889` identities are ASUS Aura
addressable-header / ROG Aura Terminal USB controllers; `0B05:1AA6` is a newer Aura mainboard
controller. All are cataloged for read-only inventory only and are sourced from the GPL-compatible
OpenRGB `AsusAuraUSBControllerDetect.cpp` detector at commit `9f82afa4`. None are write-validated:
only `0B05:19AF` has owned-hardware config-table verification, so the others stay `research-only`
until they pass the staged workflow in `contributing-hardware.md`.

## Rules

- Catalog entries are not write approval.
- New entries need stable identifiers, source provenance, and a short support note.
- Heuristic matches must remain read-only until promoted to a catalog entry.
- Write-capable support still follows `docs/hardware/contributing-hardware.md`: research note, read-only discovery, dry-run preview, then guarded write enablement.
