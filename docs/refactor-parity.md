# Refactor parity checklist

Use this checklist for behavior-preserving modernization passes.

## General

- Public C++ and QML APIs remain available.
- Return values, error strings, activity-log entries, and emitted signals remain equivalent.
- Linux and Windows configurations still generate and build.
- CTest and `all_qmllint` pass.
- Behavior-preserving refactors land as small reviewable passes with the current behavior,
  structural change, and validation listed in the pull request.

## Profiles

- Names are trimmed, empty names become `default`, and unsupported characters become `_`.
- Saves remain indented JSON and use atomic replacement.
- Format version 1 and legacy color-only zones still load.
- Devices match by ID; zones match by name and then stored index.
- Unknown devices/zones and invalid colors retain their existing handling.
- Rename collisions, missing files, malformed JSON, and unmatched profiles preserve errors.
- Profile compatibility and apply reports keep the same keys, counts, summaries, details,
  and preview item shapes for mixed success, missing device, missing zone, invalid effect,
  unsupported effect, and no-match scenarios.
- Shared profile planning remains internal. New apply or preview paths should reuse the
  existing planner instead of rebuilding profile JSON matching or report shaping.

## Daemon protocol

- Protocol version remains `1`.
- Method names and request/response JSON shapes remain unchanged.
- `hello` and `status` remain available during version mismatch.
- Request IDs remain decimal strings.
- The 1 MiB message limit and newline delimiter behavior remain unchanged.
- Unknown methods and malformed or oversized messages preserve their responses/disconnect behavior.
- Compact JSON plus one newline remains the only frame encoding.
- Exact-limit requests and responses are accepted; frames over the limit are rejected on the
  same side as before.
- Client and server frame handling remains shared through the internal codec so newline,
  empty-frame, malformed JSON, and 1 MiB-limit behavior cannot drift.
- Snapshot JSON keeps compatibility aliases and discovery fields, including `permission`,
  `permissions`, `realBackendId`, `discoveryIdentity`, `discoverySupportStage`,
  `discoverySupportStatus`, `discoverySupportFamily`, `discoverySupportNotes`,
  `discoveryCataloged`, and `discoveryWriteCapableBackend`.

## Writes and effects

- Dry-run reports success without applying hardware writes.
- Confirmation-gated writes remain blocked until confirmed and are cleared on backend reinitialization.
- Permission failures preserve their reason.
- Static, animated, local-frame, and all-off paths preserve logs and signals.
- Effect support enumeration keeps the same order: Static, Rainbow, Breathing, ColorCycle.
- Schedule-time parsing keeps invalid or empty settings normalized to `18:00` and stores
  valid values as `HH:mm`.
- Schedule firing semantics are parity-frozen in both `ProfileScheduleRunner` (GUI fallback)
  and `core/ScheduleService` (daemon): one attempt per calendar day, missed boundaries are
  skipped on startup and after every config change, and the poll timer stays clamped to
  1s-60s. The daemon persists the same `schedule/enabled`, `schedule/profile`, and
  `schedule/time` keys in its own settings scope.
- Daemon-side scheduled applies go through the unchanged permission, confirmation, and
  dry-run gates; unconfirmed hardware zones are skipped with a logged reason and
  confirmation remains in-memory and session-only.

## Hardware protocols

- Serializer results are byte-for-byte identical.
- Report count, report length, apply-bit placement, channel mapping, and summaries remain stable.
- Only allowlisted and config-verified hardware writes can be approved.
- Discovery identities, stable probe IDs, backend IDs, provider precedence, and deduplication
  behavior remain stable.

## Diagnostics

- Diagnostics schema version remains `1`.
- Diagnostics exports include the same top-level keys and continue to exclude profile contents.
- Home, temp, app-data, profile, daemon socket, and hardware path redaction remain stable.
- Setup summaries and attention flags match the QML-facing setup getters.
