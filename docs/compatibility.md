<p><strong>English</strong> | <a href="compatibility_zh.md">简体中文</a></p>

# Compatibility

Use the manifest and notes attached to the release marked **Latest** as the source of truth for the published component set and validation status.

## Current Public Matrix

| Component | Current public source |
| --- | --- |
| Android | APK attached to the latest Release |
| iOS | TestFlight entry linked from the latest Release notes |
| Desktop | Windows x64 installer or macOS Apple Silicon DMG attached to the latest Release |
| Python SDK | `watcherobot` wheel attached to the latest Release |
| ESP32-S3 + Himax | `PTL-paired` ZIP attached to the latest Release |
| SD card | `sd-resources` archive attached to the latest Release |
| STM32F103 | STM32 ZIP attached to the latest Release |
| Hardware | Current files under `hardware/` |

Package-integrity verification does not imply full on-device acceptance. Record camera, video, audio, orientation, and continuous-operation checks separately for each release.

## Setup Order

1. Download the required files from the Release page.
2. Flash STM32F103 with the STM32 ZIP from that Release.
3. Use that Release's PTL paired package to flash Himax first and ESP32-S3 second.
4. Prepare the FAT32 SD card with that Release's device resources.
5. Insert the card and power on the robot.
6. Install the required Desktop, Android, iOS, or Python SDK client from that Release.
7. Run the first-action smoke test.

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
