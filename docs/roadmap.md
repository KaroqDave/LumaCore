# LumaCore Roadmap

LumaCore is a Linux-first RGB control application built around safe development practices. The project starts with mock devices and a clean device abstraction before adding detection or hardware writes.

## Phase 1: Safe Foundation

- Build a Qt 6 desktop application with a mock RGB backend.
- Model devices, zones, LEDs, colors, and basic static color operations.
- Add profile JSON save/load with tests.
- Add basic effects: static color, breathing, and rainbow.
- Document hardware safety rules before adding real device access.

## Phase 2: Linux Integration

- Add read-only hardware discovery experiments behind explicit feature flags.
- Add structured logging for backend discovery and failures.
- Define permission handling for udev, i2c-dev, hidraw, and USB access.
- Keep the GUI unprivileged and introduce a daemon/service boundary before privileged writes.

## Phase 3: First Hardware Backends

- Prototype read-only ASUS Aura-style motherboard discovery.
- Add hidapi and libusb support where appropriate.
- Evaluate Linux SMBus/i2c-dev access only after detection, logging, permissions, and confirmation flows exist.
- Require mock coverage and tests before enabling any real write path.

## Phase 4: Extensibility

- Stabilize the backend interface.
- Add plugin-style backend loading when the core contracts are mature.
- Add profile import/export and device mapping.
- Prepare Windows support without weakening Linux safety boundaries.

## Safety Principles

- No direct hardware writes from the GUI.
- No hidden root or administrator requirement.
- No SMBus/I2C writes without read-only detection, logging, permission checks, and user confirmation.
- Prefer conservative behavior over broad device claims.
- Keep the mock backend useful enough for UI, profile, and effect development.
