# Architecture

LumaCore is currently a mock-only Qt Quick application. The goal is to build the UI, profile format, and device model safely before adding any real hardware access.

## Layers

- `app/`: application startup and Qt/QML wiring.
- `core/`: RGB data model, device abstraction, device ownership, and profile save/load.
- `backends/mock/`: safe simulated hardware backend.
- `ui/`: QML-facing models and controller.
- `ui/qml/`: Qt Quick user interface.

## Current Flow

1. `main.cpp` creates `DeviceManager`, UI models, and `AppController`.
2. `DeviceManager` asks `MockBackend` for devices.
3. `MockBackend` creates one `MockRgbDevice`.
4. QML displays devices through `DeviceListModel` and zones through `ZoneListModel`.
5. Color changes go from QML to `AppController`, then to `DeviceManager`, then to the mock device.
6. Profile save/load reads and writes JSON under `./profiles`.

## Ownership

- `DeviceManager` owns devices with `std::unique_ptr<RgbDevice>`.
- Devices own their zones by value.
- Zones own LEDs by value.
- QML never owns or directly mutates hardware-shaped objects.

## Backend Boundary

`RgbDevice::setZoneStaticColor()` is the only write-like operation in Phase 1. Today it only updates `MockRgbDevice` memory. Future hardware backends must keep this boundary and add permission checks, logging, and confirmation before real writes are possible.
