# Hardware Safety

Phase 1 does not touch real hardware. LumaCore currently has no hidapi, libusb, i2c-dev, SMBus, hidraw, udev, root, or administrator code.

## Current Guarantees

- The GUI talks only to QML-facing models and `AppController`.
- `DeviceManager` owns mock devices only.
- Static color changes update in-memory mock zones.
- Mock changes are logged to the console and UI log.
- Profiles load by applying colors through the same mock backend path used by the UI.

## Future Hardware Rules

Before adding any real hardware writes:

- Add read-only discovery first.
- Add structured logging for discovery and write attempts.
- Add explicit permission detection and clear user-facing errors.
- Keep the GUI unprivileged.
- Add a daemon/service boundary before privileged writes.
- Require user confirmation before any potentially risky write path.
- Add tests for profile and effect behavior using the mock backend first.

## SMBus/I2C Rule

No SMBus or Linux `i2c-dev` writes should be implemented until read-only detection, logging, permissions handling, user confirmation, and a mock-tested control path exist.
