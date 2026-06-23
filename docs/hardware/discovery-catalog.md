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

## Rules

- Catalog entries are not write approval.
- New entries need stable identifiers, source provenance, and a short support note.
- Heuristic matches must remain read-only until promoted to a catalog entry.
- Write-capable support still follows `docs/hardware/contributing-hardware.md`: research note, read-only discovery, dry-run preview, then guarded write enablement.
