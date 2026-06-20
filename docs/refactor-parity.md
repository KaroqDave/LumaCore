# Refactor parity checklist

Use this checklist for behavior-preserving modernization passes.

## General

- Public C++ and QML APIs remain available.
- Return values, error strings, activity-log entries, and emitted signals remain equivalent.
- Linux and Windows configurations still generate and build.
- CTest and `all_qmllint` pass.

## Profiles

- Names are trimmed, empty names become `default`, and unsupported characters become `_`.
- Saves remain indented JSON and use atomic replacement.
- Format version 1 and legacy color-only zones still load.
- Devices match by ID; zones match by name and then stored index.
- Unknown devices/zones and invalid colors retain their existing handling.
- Rename collisions, missing files, malformed JSON, and unmatched profiles preserve errors.

## Daemon protocol

- Protocol version remains `1`.
- Method names and request/response JSON shapes remain unchanged.
- `hello` and `status` remain available during version mismatch.
- Request IDs remain decimal strings.
- The 1 MiB message limit and newline delimiter behavior remain unchanged.
- Unknown methods and malformed or oversized messages preserve their responses/disconnect behavior.

## Writes and effects

- Dry-run reports success without applying hardware writes.
- Confirmation-gated writes remain blocked until confirmed and are cleared on backend reinitialization.
- Permission failures preserve their reason.
- Static, animated, local-frame, and all-off paths preserve logs and signals.

## Hardware protocols

- Serializer results are byte-for-byte identical.
- Report count, report length, apply-bit placement, channel mapping, and summaries remain stable.
- Only allowlisted and config-verified hardware writes can be approved.
