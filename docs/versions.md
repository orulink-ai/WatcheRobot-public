# Version Tracking

Use this document to find the source of truth for each public WatcheRobot component.

## Current Sources

| Component | Current public source | Version signal | Notes |
| --- | --- | --- | --- |
| WatcheRobot repository package | Root `VERSION` | `0.1.0` | Current public source package version for this repository. |
| Repository default branch | GitHub `main` | Git commit SHA | Public release readiness is gated by CI and release assets. |
| Python SDK | `python-sdk/pyproject.toml` | Python package version | Public source is included directly in this repository export. |
| ESP32-S3 firmware | GitHub Release asset | Release package version | Firmware source is not published in this repository. |
| ESP32 behavior assets | GitHub Release asset | SD-card asset package version | Must match the firmware release package. |
| STM32F103 firmware | GitHub Release asset | Release package version | Firmware source is not published in this repository. |
| Hardware PCB package | filenames under `hardware/pcb/` | dated exports, mostly 2026-06-11 to 2026-06-18 | BOM, CPL, schematic, layout, and Gerber files should be kept in sync by board. |
| Mechanical model | `hardware/3d-models/exports/WatcherRobot-mian.stp` | current STEP export | Current public mechanical assembly model. |
| App, Server, Desktop | GitHub Release assets when available | release asset version | Source code is not part of this repository. |

## Release Version Rules

- Every public release must update `docs/compatibility.md`.
- Firmware releases must state the ESP32 firmware version, STM32 firmware version, SD-card asset version, and hardware compatibility.
- Full product releases must include App, Server, Desktop, ESP32, STM32, SD-card asset, and hardware compatibility notes when those assets are included.
- Release artifacts must include `SHA256SUMS.txt`.

## Known Current State

- Current public source package version is `0.1.0`.
- Public firmware source has been removed from this repository boundary.
- `docs/compatibility.md` records the open compatibility matrix format.
