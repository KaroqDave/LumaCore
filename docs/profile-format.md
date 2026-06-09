# Profile Format

Profiles are JSON files stored under `./profiles` for now. The filename is the normalized profile name plus `.json`.

Example:

```json
{
    "formatVersion": 1,
    "application": "LumaCore",
    "profileName": "default",
    "devices": [
        {
            "id": "mock-asus-tuf-x870-plus-wifi",
            "name": "Mock ASUS TUF X870-PLUS WIFI",
            "vendor": "ASUS",
            "type": "Motherboard",
            "zones": [
                {
                    "name": "Motherboard",
                    "type": "Motherboard",
                    "ledCount": 10,
                    "color": "#4080FF",
                    "rgb": {
                        "red": 64,
                        "green": 128,
                        "blue": 255,
                        "hex": "#4080FF"
                    }
                }
            ]
        }
    ]
}
```

## Load Rules

- Devices are matched by `id`.
- Zones are matched by `name`.
- Colors are loaded from the zone `color` hex string.
- Unknown devices or zones are skipped.
- Invalid colors are skipped and reported in the UI log.

This format is intentionally simple and may evolve while the project is still mock-only.
