<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# HIDAPI Vendor Notes

LumaCore vendors the Windows HIDAPI backend from upstream `libusb/hidapi`
release `hidapi-0.15.0`.

Source:
https://github.com/libusb/hidapi/releases/tag/hidapi-0.15.0

Vendored files:

- `hidapi/hidapi.h`
- `windows/hid.c`
- `windows/hidapi_*.h`
- `windows/hidapi_descriptor_reconstruct.c`
- upstream license files and README

LumaCore builds these files as a static internal target on Windows when no
system HIDAPI package is found and `LUMACORE_USE_BUNDLED_HIDAPI` is enabled.
Linux builds keep using distribution packages by default.
