# Compatibility

Use this document to record known-good version sets. Every GitHub Release should update this table before publication.

## Current Public Matrix

| WatcheRobot release | App | Server | Desktop | ESP32-S3 firmware | SD-card behavior assets | STM32F103 firmware | Hardware |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `0.1.0` source package | Release asset when available | Release asset when available | Release asset when available | Release asset when available | Release asset when available | Release asset when available | PCB package dated 2026-06-11 to 2026-06-18 |

## Release Checklist

Before publishing a release:

- Set the WatcheRobot release tag.
- Record App, Server, and Desktop asset versions if those assets are included.
- Record ESP32 firmware release package version.
- Record SD-card behavior asset package version.
- Record STM32 firmware release package version.
- Record hardware package version and any required assembly notes.
- Add upgrade order and incompatibilities when needed.

See `docs/versions.md` for the version source-of-truth map.
