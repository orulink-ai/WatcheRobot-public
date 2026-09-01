# SDK and Public Interface Boundary

The public software integration surface of this repository is the root-level Python SDK plus the runtime assets distributed through GitHub Releases. ESP32-S3 and STM32F103 firmware source code is not published here.

## Current Public Interfaces

| Area | Entry point | Status |
| --- | --- | --- |
| Python SDK | `python-sdk/` | Public source, examples, and tests for host-side control. |
| SDK protocol notes | `python-sdk/docs/protocol-v1.md` | Public SDK-facing protocol notes. |
| Factory resource IDs | `python-sdk/docs/resources.md` | Public reference for supported built-in behavior, animation, and audio IDs. |
| ESP32-S3 release flashing | `tools/win_flasher/`, `tools/flash-release.cmd` | Public helper for prebuilt release ZIPs. |
| AI flashing skill | `tools/flashing/` | Public AI-readable workflow for firmware flashing and boot checks. |
| Release assets | GitHub Releases | Prebuilt firmware, SD-card assets, installers, manifest, and checksums. |

## Stable for Users

Users may rely on:

- repository layout documented in `README.md`
- SDK installation and examples in `python-sdk/README.md`
- release assets being distributed through GitHub Releases
- SD-card behavior assets being copied under `anim/`
- hardware manufacturing files under `hardware/`

## Not Published Here

The following implementation details are outside this public repository:

- ESP32-S3 firmware source and internal component layout
- STM32F103 firmware source and host tests
- App, server, and desktop source code
- private bring-up logs, local serial-port records, and bench-only scripts

## Development Against the Public Repo

- For SDK integration, use `python-sdk/`.
- For hardware reproduction, use `hardware/pcb/` and `hardware/3d-models/`.
- For flashing automation, use `tools/win_flasher` or the AI-readable workflow under `tools/flashing/`.
- For firmware behavior, use the documented Release assets and verify device capabilities at runtime.
