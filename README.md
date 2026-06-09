# LumaCore

LumaCore is an open-source RGB control application being built Linux first with C++23, Qt 6, and CMake. It is a passion project focused on safe, maintainable RGB control rather than quick hardware hacks.

This first milestone is intentionally mock-only. It creates a Qt Quick app, a core RGB model, and a simulated ASUS motherboard-style device with three zones:

- Motherboard
- ARGB Header 1
- ARGB Header 2

No real hardware access, root permissions, SMBus/I2C writes, USB writes, or hidraw access are used.

## Build on Linux

Install a C++ compiler, CMake, Ninja or Make, and Qt 6 development packages. Package names vary by distribution. On Arch-based systems, the core dependencies are typically:

```sh
sudo pacman -S cmake gcc qt6-base qt6-declarative
```

Configure and build:

```sh
cmake -S . -B build
cmake --build build
```

Run:

```sh
./build/lumacore
```

## Current Scope

- Qt Quick desktop window.
- In-memory RGB device model.
- Mock backend only.
- Static color setting for mock zones.

See `docs/roadmap.md` for planned profile, effect, daemon, and hardware safety work.
